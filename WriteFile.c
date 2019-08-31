/*
Copyright 2019 TIKM(github:HITIKM)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*
将弹幕池写到ass文件 
参数：
输入文件/输出文件/分辨率宽/分辨率高/字号/字体/
阴影/描边/滚动速度（滚动弹幕）/停留时间（现隐弹幕）/密度/不透明度/下部留空/
屏蔽（右左/左右/顶部/底部/特殊/彩色/重复）/调试模式（数据表格/分布图）NULL表示暂不开启debug模式/
返回值：
0 正常退出
1 弹幕池为空
2 文件创建失败
3 4 5 6 7 申请内存空间失败
8 写文件时发生错误 
*/
int writeAss(const char *opFileName, DANMAKU *head, const int resX, int resY, const int fontSize,
			 char *fontName,const int shadow, const int outline, const float rollTime, const float holdTime, 
			 const int density, const int opacity, const int blank, char *blockMode, char *debugMode)
{
	if(head == NULL)
	{
		#if PRINT_ERR == TRUE
		printf("\n[X] 弹幕池为空");
		#endif
		return 1;
	}
	
	FILE *opF;
	DANMAKU *now = NULL, *last = NULL;
	int cnt;
	
	/*创建文件*/
	if((opF = fopen(opFileName, "w")) == NULL)
	{
		#if PRINT_ERR == TRUE
		printf("\n[X] 文件创建失败");
		#endif
		return 2;
	}
	
	char tempText[MAX_TEXT_LENGTH];
	DANMAKU *signPtr = head, *scanPtr = head;
	
	int screenNum;
	int listCnt, densityCnt, PositionY;
	int textLen, textHei;
	
	float *R2LToRightTime, *R2LToLeftTime;	/*右左滚动行经过特定点时间*/
	float *L2RToRightTime, *L2RToLeftTime;	/*左右滚动行经过特定点时间*/
	float *fixEndTime;	/*顶部与底部弹幕消失时间*/
	
	if((R2LToRightTime = (float *)malloc(resY * sizeof(float))) == NULL)
	{
		#if PRINT_ERR == TRUE
		printf("\n[X] 申请内存空间失败");
		#endif
		fclose(opF);
		return 3;
	}
	if((R2LToLeftTime = (float *)malloc(resY * sizeof(float))) == NULL)
	{
		free(R2LToRightTime);
		#if PRINT_ERR == TRUE
		printf("\n[X] 申请内存空间失败");
		#endif
		fclose(opF);
		return 4;
	}
	if((L2RToRightTime = (float *)malloc(resY * sizeof(float))) == NULL)
	{
		free(R2LToRightTime);
		free(R2LToLeftTime);
		#if PRINT_ERR == TRUE
		printf("\n[X] 申请内存空间失败");
		#endif
		fclose(opF);
		return 5;
	}
	if((L2RToLeftTime = (float *)malloc(resY * sizeof(float))) == NULL)
	{
		free(R2LToRightTime);
		free(R2LToLeftTime);
		free(L2RToRightTime);
		#if PRINT_ERR == TRUE
		printf("\n[X] 申请内存空间失败");
		#endif
		fclose(opF);
		return 6;
	}
	if((fixEndTime = (float *)malloc(resY * sizeof(float))) == NULL)
	{
		free(R2LToRightTime);
		free(R2LToLeftTime);
		free(L2RToRightTime);
		free(L2RToLeftTime);
		#if PRINT_ERR == TRUE
		printf("\n[X] 申请内存空间失败");
		#endif
		fclose(opF);
		return 7;
	}
	
	/*写ass头*/
	char hexOpacity[3];
	toHexOpacity(255 - opacity, hexOpacity);
	fprintf(opF, "[Script Info]\n"
				 "ScriptType: v4.00+\n"
				 "Collisions: Normal\n"
				 "PlayResX: %d\n"
				 "PlayResY: %d\n"
				 "Timer: 100.0000\n\n" 
				 "[V4+ Styles]\n"
				 "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
				 "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
				 "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
				 "MarginL, MarginR, MarginV, Encoding\n"
				 "Style: Default,%s,%d,&H%sFFFFFF,&H1FFFFFF,&H1000000,"
				 "&H1E6A5149,0,0,0,0,100.00,100.00,0.00,0.00,1,%d,%d,8,0,0,0,1",
				resX, resY, fontName, fontSize, hexOpacity, outline, shadow);
	fprintf(opF, "\n\n[Events]\n"
				 "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text");
	
	if(blank != 0)
	{
		resY -= blank;
	}
	
	now = head;
	listCnt = 0;
	while(now != NULL)
	{/*读链表写ass*/
		listCnt++;
		 
		/*按类型屏蔽及颜色屏蔽*/
		if( (blockMode[0] != '0' && now -> type == 1) || (blockMode[1] != '0' && now -> type == 2) ||
		    (blockMode[2] != '0' && now -> type == 3) || (blockMode[3] != '0' && now -> type == 4) ||
		    (blockMode[4] != '0' && now -> type == 5) || (blockMode[5] != '0' && now -> color != 0xFFFFFF) )
		{/*整体类型屏蔽*/
			now -> type *= -1;
			goto NEXTNODE;
		}
		
		/*密度上限屏蔽 及 相同内容弹幕屏蔽*/
		if(density > 0 || blockMode[6] != '0')
		{
			while(getEndTime(signPtr, rollTime, holdTime) < now -> time + TIME_EPS)
			{/*移动指针到同屏第一条弹幕*/
				signPtr = signPtr -> next;
			}
			
			scanPtr = signPtr;
			screenNum = 0;
			while(scanPtr != now)
			{ 
				if(scanPtr -> type > 0 && getEndTime(scanPtr, rollTime, holdTime) > now -> time)
				{
					if(IS_NORMAL(scanPtr) && now -> type > 0 &&
					   blockMode[6] != '0' && !strcmp(scanPtr -> text, now -> text))
					{/*如果重复内容屏蔽开启且内容重复*/
						now -> type *= -1;
						goto NEXTNODE;
					}
					
					if(IS_NORMAL(scanPtr) || IS_SPECIAL(scanPtr))
					{/*如果是一个合法的弹幕*/
						screenNum++;
					}
				}
				scanPtr = scanPtr -> next;
			}
			
			if(now -> type > 0 && density > 0 && density <= screenNum)
			{/*判断是否达到弹幕密度要求上限并屏蔽*/
				now -> type *= -1;
				//goto NEXTNODE;
			}
		}
		
		textLen = getStrLen(now -> text, fontSize, now -> fontSize, fontName);
		textHei = getStrHei(now -> text, fontSize, now -> fontSize, fontName);
		if(now -> type == 1)/*右左弹幕*/ 
		{
			for(PositionY = 1; PositionY < resY - textHei; PositionY++)
			{
				for(cnt = 0; cnt < fontSize; cnt++)
				{
					if(now->time < R2LToRightTime[PositionY + cnt] || 
					   now->time + rollTime*(float)resX/(resX+textLen) < R2LToLeftTime[PositionY + cnt])
					{/*当本条弹幕出现该行最后一条弹幕未离开屏幕右边 或 
						当本条弹幕到达左端时该行最后一条弹幕没有完全退出屏幕*/
						PositionY = PositionY + cnt + 1;
						break;
					}
				}
				if(cnt >= textHei)
				{
					break;
				}
			}
			if(PositionY >= resY - textHei)
			{
				if(density == -1)
				{
					now -> type == -1;
					goto NEXTNODE;
				}
				PositionY = findMin(R2LToRightTime, resY, resY - textHei, 0);
			}
			for(cnt = 0; cnt < textHei; cnt++)
			{
				R2LToRightTime[PositionY + cnt] = now -> time + rollTime * (float)textLen / (resX + textLen); 
				R2LToLeftTime[PositionY + cnt] = now -> time + rollTime;
			}
			
			fprintf(opF, "\nDialogue: 0,");
			printTime(opF, now->time, ",");
			printTime(opF, now->time + rollTime, ",");
			fprintf(opF, "Default,,0000,0000,0000,,{\\move(%d,%d,%d,%d)\\q2",
					resX + textLen/2, PositionY, -1 * textLen / 2, PositionY);
			
			if(now -> fontSize != 25)
			{
				fprintf(opF, "\\fs%d", textHei);
			}
			
			if(now -> color != 0xFFFFFF)
			{
				char hexColor[7];
				fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
			}
			
			fprintf(opF, "}%s", now->text);
		}
		else if(now -> type == 2)/*左右弹幕*/ 
		{
			for(PositionY = 1; PositionY < resY - textHei; PositionY++)
			{
				for(cnt = 0; cnt < textHei; cnt++)
				{
					if(now->time < L2RToRightTime[PositionY + cnt] || 
					   now->time + rollTime*(float)resX/(resX+textLen) < L2RToLeftTime[PositionY + cnt])
					{/*当本条弹幕出现该行最后一条弹幕未离开屏幕左边 或 
						当本条弹幕到达右端时该行最后一条弹幕没有完全退出屏幕*/
						PositionY = PositionY + cnt + 1;
						break;
					}
				}
				if(cnt >= textHei)
				{
					break;
				}
			}
			if(PositionY >= resY - textHei)
			{
				if(density == -1)
				{
					now -> type == -2;
					goto NEXTNODE;
				}
				PositionY = findMin(L2RToRightTime, resY, resY - textHei, 0);
			}
			for(cnt = 0; cnt < textHei; cnt++)
			{
				L2RToRightTime[PositionY + cnt] = now -> time + rollTime * (float)textLen / (resX + textLen); 
				L2RToLeftTime[PositionY + cnt] = now -> time + rollTime;
			}
			
			fprintf(opF, "\nDialogue: 0,");
			printTime(opF, now->time, ",");
			printTime(opF, now->time + rollTime, ",");
			fprintf(opF, "Default,,0000,0000,0000,,{\\move(%d,%d,%d,%d)\\q2",
					-1 * textLen / 2, PositionY, resX + textLen/2, PositionY);
			
			if(now -> fontSize != 25)
			{
				fprintf(opF, "\\fs%d", textHei);
			}
			
			if(now -> color != 0xFFFFFF)
			{
				char hexColor[7];
				fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
			}
			
			fprintf(opF, "}%s", now->text);
		}
		else if(now -> type == 3)/*顶端弹幕*/ 
		{
			for(PositionY = 1; PositionY < resY - textHei; PositionY++)
			{
				for(cnt = 0; cnt < textHei; cnt++)
				{
					if(now->time < fixEndTime[PositionY + cnt])
					{/*当本条弹幕出现时本行上一条弹幕还没有消失*/
						PositionY = PositionY + cnt + 1;
						break;
					}
				}
				if(cnt >= textHei)
				{
					break;
				}
			}
			if(PositionY >= resY - textHei)
			{
				if(density == -1)
				{
					now -> type == -3;
					goto NEXTNODE;
				}
				PositionY = findMin(fixEndTime, resY, resY - textHei, 0);
			}
			for(cnt = 0; cnt < textHei; cnt++)
			{
				fixEndTime[PositionY + cnt] = now -> time + holdTime;
			}
			
			fprintf(opF, "\nDialogue: 0,");
			printTime(opF, now->time, ",");
			printTime(opF, now->time + holdTime, ",");
			fprintf(opF, "Default,,0000,0000,0000,,{\\pos(%d,%d)\\q2", resX / 2, PositionY);
			
			if(now -> fontSize != 25)
			{
				fprintf(opF, "\\fs%d", textHei);
			}
			
			if(now -> color != 0xFFFFFF)
			{
				char hexColor[7];
				fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
			}
			
			fprintf(opF, "}%s", now->text);
		}
		else if(now -> type == 4)/*底端弹幕*/ 
		{
			for(PositionY = resY - 1; PositionY > textHei - 1; PositionY--)
			{
				for(cnt = 0; cnt < textHei; cnt++)
				{
					if(now->time < fixEndTime[PositionY - cnt])
					{/*当本条弹幕出现时本行上一条弹幕还没有消失*/
						PositionY = PositionY - cnt - 1;
						break;
					}
				}
				if(cnt >= textHei)
				{
					break;
				}
			}
			if(PositionY < textHei)
			{
				if(density == -1)
				{
					now -> type == -4;
					goto NEXTNODE;
				}
				PositionY = findMin(fixEndTime, resY, textHei, 1);
			}
			for(cnt = 0; cnt < textHei; cnt++)
			{
				fixEndTime[PositionY - cnt] = now -> time + holdTime;
			}
			
			fprintf(opF, "\nDialogue: 0,");
			printTime(opF, now->time, ",");
			printTime(opF, now->time + holdTime, ",");
			fprintf(opF, "Default,,0000,0000,0000,,{\\pos(%d,%d)\\q2",
					resX / 2, PositionY - textHei + 2);
			
			if(now -> fontSize != 25)
			{
				fprintf(opF, "\\fs%d", textHei);
			}
			
			if(now -> color != 0xFFFFFF)
			{
				char hexColor[7];
				fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
			}
			
			fprintf(opF, "}%s", now->text);
		}
		else if(now -> type == 5)/*特殊弹幕*/
		{
			char *n7Ptr = now -> text;
			float n7ExistTime;
			int n7MoveTime, n7PauseTime;
			float n7StartX, n7StartY, n7EndX, n7EndY;
			int n7FadeStart, n7FadeEnd, n7FrY, n7FrZ;
			char n7Temp[MAX_TEXT_LENGTH], n7FontName[256];
			while(*n7Ptr != '[' && *n7Ptr++ !='\0')
			{
				n7Ptr++;
			}
			n7Ptr++;
			/*[0,0.17,"1-1",7,"文本部分内容",0,0,0,0.17,500,0,true,"微软雅黑",1]*/
			n7StartX = strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7StartY = strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7FadeStart = (int)((1 - strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, '-')))) * 255);		
			n7FadeEnd = (int)((1 - strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')))) * 255);	
			n7ExistTime = strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			
			/*收集文本部分*/
			while(*(n7Ptr - 1) != '\"' && *n7Ptr != '\0' && !(*(n7Ptr - 6) == '&' && *(n7Ptr - 5) == 'q' && 
				  *(n7Ptr - 4) == 'u' && *(n7Ptr - 3) == 'o' && *(n7Ptr - 2) == 't' && *(n7Ptr - 1) == ';'))
			{/*将指针移到引号或其转义&quot;之后*/
				n7Ptr++;
			}
			cnt = 0;
			while(*n7Ptr != '\"' && *n7Ptr != '\0' &&  !(*n7Ptr == '&' && *(n7Ptr + 1) == 'q' && 
				   *(n7Ptr + 2) == 'u' && *(n7Ptr + 3) == 'o' && *(n7Ptr + 4) == 't' && *(n7Ptr + 5) == ';') )
			{/*在引号或其转义&quot;之前停止拷贝*/
				if(*n7Ptr == '/' && *(n7Ptr + 1) == 'n')
				{/*在xml弹幕文件中/n为换行符，ass中\N为强制换行符，只有特殊弹幕换行符生效*/ 
					tempText[cnt] = '\\';
					tempText[cnt + 1] = 'N';
					cnt++;
					n7Ptr++;
				}
				else
				{
					tempText[cnt] = *n7Ptr;
				}
				cnt++;
				n7Ptr++;
			}
			tempText[cnt] = '\0';
			while(*(n7Ptr - 1) != ',' && *n7Ptr !='\0')
			{
				n7Ptr++;
			}
			
			n7FrZ = strToInt(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7FrY = strToInt(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7EndX = strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7EndY = strToFloat(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7MoveTime = strToInt(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			n7PauseTime = strToInt(deQuotMarks(strGetLeftPart(n7Temp, &n7Ptr, ',')));
			
			while(*n7Ptr != ',' && *n7Ptr != '\0')
			{
				n7Ptr++;
			}
			
			/*字体信息 收集方法同文本部分*/ 
			while(*(n7Ptr - 1) != '\"' && *n7Ptr != '\0' &&  !(*(n7Ptr - 6) == '&' && *(n7Ptr - 5) == 'q' && 
				  *(n7Ptr - 4) == 'u' && *(n7Ptr - 3) == 'o' && *(n7Ptr - 2) == 't' && *(n7Ptr - 1) == ';'))
			{
				n7Ptr++;
			}
			cnt = 0;
			while(*n7Ptr != '\"' && *n7Ptr != '\0' && cnt < 512 && !(*n7Ptr == '&' && *(n7Ptr + 1) == 'q' && 
				  *(n7Ptr + 2) == 'u' && *(n7Ptr + 3) == 'o' && *(n7Ptr + 4) == 't' && *(n7Ptr + 5) == ';'))
			{
				n7FontName[cnt] = *n7Ptr;
				cnt++;
				n7Ptr++;
			}
			n7FontName[cnt] = '\0';
			
			fprintf(opF, "\nDialogue: 0,");
			printTime(opF, now->time, ",");
			printTime(opF, now->time + n7ExistTime, ",");
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7");
			if( (n7StartX - 1 < TIME_EPS && n7StartX > TIME_EPS) || (n7EndX - 1 < TIME_EPS && n7EndX > TIME_EPS)||
				(n7StartY - 1 < TIME_EPS && n7StartY > TIME_EPS) || (n7EndY - 1 < TIME_EPS && n7EndY > TIME_EPS) )
			{
				n7StartX *= resX;
				n7EndX *= resX;
				n7StartY *= resY;
				n7EndY *= resY;
			}
			if(n7StartX == n7EndX && n7StartY == n7EndY)
			{
				fprintf(opF, "\\pos(%d,%d)", (int)n7StartX, (int)n7StartY);
			}
			else
			{
				fprintf(opF, "\\move(%d,%d,%d,%d", (int)n7StartX, (int)n7StartY, (int)n7EndX, (int)n7EndY);
				if(n7PauseTime == 0)
				{
					if(n7MoveTime == 0)
					{
						fprintf(opF, ")");
					}
					else
					{
						fprintf(opF, ",0,%d)", n7MoveTime);
					}
				}
				else
				{
					fprintf(opF, ",%d,%d)", n7PauseTime, n7MoveTime + n7PauseTime);
				}
			}
			fprintf(opF, "\\q2");
			
			if(now -> fontSize != 25)
			{
				fprintf(opF, "\\fs%d", fontSize + (now -> fontSize - 25) );
			}
			
			if(*n7Temp != '\0')
			{
				fprintf(opF, "\\fn%s", n7FontName);
			}
			
			if(now -> color != 0xFFFFFF)
			{
				char hexColor[7];
				fprintf(opF, "\\c&H%s", toHexColor(now->color, hexColor));
			}
			
			if(n7FrY != 0)
			{
				fprintf(opF, "\\fry%d", n7FrY);
			}
			if(n7FrZ != 0)
			{
				fprintf(opF, "\\frz%d", n7FrZ);
			}
			
			if(n7FadeStart != 0 || n7FadeEnd != 0)
			{/*fade(淡入透明度,实体透明度,淡出透明度,淡入开始时间,淡入结束时间,淡出开始时间,淡出结束时间)*/
				fprintf(opF, "\\fade(%d,%d,%d,0,0,%d,%d)", n7FadeStart, n7FadeStart,
						n7FadeEnd, n7PauseTime, (int)(n7ExistTime * 1000));
			}
			else
			{
				fprintf(opF, "\\alpha&H00");
			}
			
			fprintf(opF, "}%s", tempText);
		}
		else if(now -> type == 8)/*代码弹幕*/ 
		{
			#if PRINT_ERR == TRUE
			printf("\n[!] 第%d条 无法解析代码弹幕", listCnt);
			#endif
			fprintf(opF, "\nDialogue: 0,0:00:00.00,0:00:00.00,Default,"
					"NO.%d(Code danmaku):unable to read this type,0000,0000,0000,,", listCnt);
		}
		else if(now -> type > 0)/*错误弹幕*/ 
		{
			#if PRINT_ERR == TRUE
			printf("\n[!] 第%d条 弹幕类型错误", listCnt);
			#endif
			fprintf(opF, "\nComment: NO.%d:unknow type", listCnt);
		}
		
		if(ferror(opF))
		{
			#if PRINT_ERR == TRUE
			printf("\n[X] 写出文件时发生错误");
			#endif
			fclose(opF);
			return 8;
		}
		
		NEXTNODE:
		now = now -> next;
	}
	/*归还空间*/
	free(R2LToRightTime);
	free(R2LToLeftTime);
	free(L2RToRightTime);
	free(L2RToLeftTime);
	free(fixEndTime);
	
	fflush(opF);
	fclose(opF);
	
	/*可以在写ass的时候顺便写debug数据 也可以单独调用writeAssDebugPath函数*/
	if(debugMode != NULL && (debugMode[0] != '0' || debugMode[1] != '0'))
	{
		return 9 + writeAssDebugPath(opFileName, head, debugMode, rollTime, holdTime, blockMode);
	}
	return 0;
}

/*
写ass字幕的debug部分 
参数：
输出文件名/链表头/模式（统计表/分布图/标签）/分辨率宽/分辨率高/滚动速度（滚动弹幕）/停留时间（现隐弹幕）/
字号/字体(utf-8)/屏蔽（右左/左右/顶部/底部/特殊/彩色/重复） 
返回值：
0 正常退出
1 链表为空
2 追加文件失败 
*/ 
int writeAssDebugPath(const char *opFileName, DANMAKU *head, char *mode, const int rollTime, const int holdTime,
					  char *blockMode)
{
	if(mode[0] == '0' && mode[1] == '0')
	{
		return 0;
	}
	
	#if PRINT_ERR == TRUE
	printf("\n[!]调试模式已开启");
	#endif
	
	if(head == NULL)
	{
		return 1;
	}
	
	FILE *opF;
	DANMAKU *now = NULL, *last = NULL;
	int cnt;
	
	/*打开文件*/
	if((opF = fopen(opFileName, "a+")) == NULL)
	{
		return 2;
	}
	
	/*下列数组下标表示弹幕类型，其中下标0表示总数，其值记录了弹幕的条数，做成数组便于利用循环*/ 
	int screen[NUM_OF_TYPE + 1] = {0};/*同屏弹幕*/ 
	int block[NUM_OF_TYPE + 1] = {0};/*被屏蔽的弹幕*/ 
	int count[NUM_OF_TYPE + 1] = {0};/*未屏蔽弹幕计数器*/ 
	int total[NUM_OF_TYPE + 1] = {0};/*总弹幕数量*/
	
	int pointShiftY = 0;/*Y轴相对标准位置向下偏移的高度*/
	float startTime, endTime;/*debug本条记录的开始与结束时间*/
	float lastDanmakuStartTime = 0.00, lastDanmakuEndTime = 0.00;/*最后一条弹幕的出场与消失的时间*/
	
	DANMAKU *signPtr = NULL, *scanPtr = NULL;
	
	fprintf(opF, "\n\nComment:DebugPath");
	fprintf(opF, "\nComment:MODE:%s", mode);
	
	if(mode[0] == '0')
	{/*如果不需要数据框，分布图就要上移*/ 
		pointShiftY = -255; 
	}
	
	/*画弹幕分布图*/
	if(mode[1] != 0)
	{
		int mPointX;/*画图时x坐标的变量*/
		
		int chartMaxHeight = 0;/*图表最高点的高度*/ 
		int allCnt[212] = {0}, blockCnt[212] = {0};
		 
		BOOL drawBlockChart = FALSE;
		
		/*计算各类弹幕总数*/
		now = head;
		while(now != NULL)
		{
			if(abs(now -> type) <= 5)
			{
				total[abs(now -> type)]++;
			}
			
			if(now -> next == NULL)
			{
				lastDanmakuStartTime = now -> time;
				lastDanmakuEndTime = getEndTime(now, rollTime, holdTime);
			}
			now = now -> next;
		}
		/*使用0位置存储总数*/
		total[0] = total[1] + total[2] + total[3] + total[4] + total[5];
		
		endTime = lastDanmakuStartTime + 30;
		/*根据偏移量选择正确的分布图框*/
		if(pointShiftY == 0)
		{
			fprintf(opF, "\nDialogue: 5,0:00:00.00,");
			printTime(opF, endTime, ",");
			/*底框*/ 
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\1a&H35\\shad0\\bord0\\p1}m 20 290"
						 " b 20 284 29 275 35 275 l 430 275 b 436 275 445 284 445 290 l 445 370"
						 " b 445 376 436 385 430 385 l 35 385 b 29 385 20 376 20 370 l 20 290{\\p0}");
			
			fprintf(opF, "\nDialogue: 6,0:00:00.00,");
			printTime(opF, lastDanmakuEndTime, ",");
			/*移动的进度条*/
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\move(20, 275, 444, 275)\\clip(m 20 290 b 20 284 29"
						 " 275 35 275 l 430 275 b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385"
						 " l 35 385 b 29 385 20 376 20 370 l 20 290)\\1c&HCECEF3\\1a&H35\\shad0\\bord0\\p1}"
						 "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
			fprintf(opF, "\nDialogue: 6,");
			printTime(opF, lastDanmakuEndTime, ",");
			printTime(opF, endTime, ",");
			/*进度条满之后的样子*/
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\pos(445, 275)\\clip(m 20 290 b 20 284 29 275 35 275"
						 " l 430 275 b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
						 " b 29 385 20 376 20 370 l 20 290)\\1c&HCECEF3\\1a&H35\\shad0\\bord0\\p1}"
						 "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
		}
		else if(pointShiftY == -255)
		{
			fprintf(opF, "\nDialogue: 5,0:00:00.00,");
			printTime(opF, endTime, ",");
			/*底框*/ 
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\1a&H35\\shad0\\bord0\\p1}m 20 35"
						 " b 20 29 29 20 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115"
						 " b 445 121 436 130 430 130 l 35 130 b 29 130 20 121 20 115 l 20 35{\\p0}");
			
			fprintf(opF, "\nDialogue: 6,0:00:00.00,");
			printTime(opF, lastDanmakuEndTime, ",");
			/*移动的进度条*/
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\move(20, 20, 444, 20)\\clip(m 20 35 b 20 29 29 20"
						 " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
						 " b 29 130 20 121 20 115 l 20 35)\\1c&HCECEF3\\1a&H35\\shad0\\bord0\\p1}"
						 "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
			fprintf(opF, "\nDialogue:6,");
			printTime(opF, lastDanmakuEndTime, ",");
			printTime(opF, endTime, ",");
			/*进度条满之后的样子*/
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\pos(445, 20)\\clip(m 20 35 b 20 29 29 20"
						 " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
						 " b 29 130 20 121 20 115 l 20 35)\\1c&HCECEF3\\1a&H35\\shad0\\bord0\\p1}"
						 "m 0 0 l -425 0 l -425 110 l 0 110 l 0 0{\\p0}");
		}
		
		
		now = head;
		while(now != NULL)
		{/*统计各时间段弹幕数量*/
			for(cnt = (int)(now->time / lastDanmakuEndTime * 211); 
				cnt < (int)(getEndTime(now, rollTime, holdTime) / lastDanmakuEndTime * 211); cnt++)
			{/*从开始时间写到结束时间*/
				allCnt[cnt]++;
				if(now -> type < 0)
				{
					blockCnt[cnt]++;
					drawBlockChart = TRUE;
				}
			}
			now = now -> next;
		}
		
		for(cnt = 0; cnt < 212; cnt++)
		{/*寻找最大高度*/ 
			if(allCnt[cnt] > chartMaxHeight)
			{
				chartMaxHeight = allCnt[cnt];
			}
		}
		
		/*画全部弹幕分布图*/
		fprintf(opF, "\nDialogue:7,0:00:00.00,");
		printTime(opF, endTime, ",");
		if(pointShiftY == 0)
		{/*分布图起始段*/ 
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\clip(m 20 290 b 20 284 29 275 35 275 l 430 275"
						 " b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
						 " b 29 385 20 376 20 370 l 20 290)\\1c&HFFFFFF\\1a&H35\\shad0\\bord0\\p1}m 20 385");
		}
		else if(pointShiftY == -255)
		{
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\clip(m 20 35 b 20 29 29 20"
						 " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
						 " b 29 130 20 121 20 115 l 20 35)\\1c&HFFFFFF\\1a&H35\\shad0\\bord0\\p1}m 20 130");
		}
		
		mPointX = 21;
		for(cnt = 0; cnt < 212; cnt++)
		{/*根据统计数据画图，最高高度是110px*/ 
			if(cnt == 1)
			{/*第一根线*/ 
				fprintf(opF, " l %d %d",
						mPointX, 385 - (int)((float)allCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
				mPointX += 2;
			}
			
			/*横向画线让柱子有一定宽度*/ 
			fprintf(opF, " l %d %d",
						mPointX, 385 - (int)((float)allCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
			
			
			if(cnt == 211)
			{/*最后一条线连接到图形右下角*/ 
				fprintf(opF, " l %d %d", mPointX, 385 + pointShiftY);
			}
			else
			{/*纵向画线，将线条画到下一个数据点高度*/ 
				fprintf(opF, " l %d %d",
						mPointX, 385 - (int)((float)allCnt[cnt + 1] / chartMaxHeight * 110) + pointShiftY);
				mPointX += 2;
			}
		}
		
		/*封闭图形*/
		fprintf(opF, " l 20 385{\\p0}");
		
		/*画屏蔽弹幕分布图*/ 
		if(drawBlockChart == TRUE)
		{
			mPointX = 21;
			fprintf(opF, "\nDialogue: 8,0:00:00.00,");
			printTime(opF, endTime, ",");
			
			if(pointShiftY == 0)
			{
				fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\clip(m 20 290 b 20 284 29 275 35 275 l 430 275"
							 " b 436 275 445 284 445 290 l 445 370 b 445 376 436 385 430 385 l 35 385"
							 " b 29 385 20 376 20 370 l 20 290)\\1c&HD3D3D3\\1a&H35\\shad0\\bord0\\p1}m 20 385");
			}
			else if(pointShiftY == -255)
			{
				fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\clip(m 20 35 b 20 29 29 20"
							 " 35 20 l 430 20 b 436 20 445 29 445 35 l 445 115 b 445 121 436 130 430 130 l 35 130"
							 " b 29 130 20 121 20 115 l 20 35)\\1c&HD3D3D3\\1a&H35\\shad0\\bord0\\p1}m 20 130");
			}
			for(cnt = 0; cnt < 212; cnt++)
			{/*根据统计数据画图，最高高度是110px*/ 
				if(cnt == 1)
				{/*第一根线*/ 
					fprintf(opF, " l %d %d",
							mPointX, 385 - (int)((float)blockCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
					mPointX += 2;
				}
				
				/*横向画线让柱子有一定宽度*/
				fprintf(opF, " l %d %d",
							mPointX, 385 - (int)((float)blockCnt[cnt] / chartMaxHeight * 110) + pointShiftY);
				
				if(cnt == 211)
				{/*最后一条线连接到图形右下角*/
					fprintf(opF, " l %d %d", mPointX, 385 + pointShiftY);
				}
				else
				{/*纵向画线，将线条画到下一个数据点高度*/ 
					fprintf(opF, " l %d %d",
							mPointX, 385 - (int)((float)blockCnt[cnt + 1] / chartMaxHeight * 110) + pointShiftY);
					mPointX += 2;
				}
			}
			/*封闭图形*/
			fprintf(opF, " l 20 385{\\p0}");
		}
	}
	
	/*显示弹幕统计表*/
	if(mode[0] != '0')
	{
		now = head;
		if(!total[0])
		{/*统计出每种弹幕的总数 如果之前没有计算过则计算一遍*/
			while(now != NULL)
			{
				if(abs(now -> type) <= NUM_OF_TYPE)
				{
					total[abs(now -> type)]++;
				}
				
				if(now -> next == NULL)
				{
					lastDanmakuStartTime = now -> time;
				}
				now = now -> next;
			}
			/*使用0位置存储总数*/
			total[0] = total[1] + total[2] + total[3] + total[4] + total[5];
		}
		
		/*画表格形状及上面的常驻或初始数据*/
		endTime = lastDanmakuStartTime + 30;
		fprintf(opF, "\n\nDialogue:3,0:00:00.00,");
		printTime(opF, endTime, ",");
		fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HCECEF3\\1a&H35\\shad0\\bord0\\p1}"
					 "m 20 35 b 20 26 26 20 35 20 l 430 20 b 439 20 445 29 445 35 l 445 50 l 20 50 l 20 35 "
					 "m 20 80 l 445 80 l 445 110 l 20 110 l 20 110 l 20 80 m 20 140 l 445 140 l 445 170 l 20"
					 " 170 l 20 170 l 20 140 m 20 200 l 445 200 l 445 230 l 20 230 l 20 230 l 20 200{\\p0}");
		fprintf(opF, "\nDialogue:4,0:00:00.00,");
		printTime(opF, endTime, ",");
		fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\1a&H35\\shad0\\bord0\\p1}"
					 "m 20 50 l 445 50 l 445 80 l 20 80 l 20 80 l 20 50 m 20 110 l 445 110 l 445 140 l 20 140"
					 " l 20 140 l 20 110 m 20 170 l 445 170 l 445 200 l 20 200 l 20 200 l 20 170 m 20 230"
					 " l 445 230 l 445 230 l 445 245 b 445 251 436 260 430 260 l 35 260 b 29 260 20 251 20 245"
					 " l 20 230{\\p0}");
		
		/*显示表格文字 注*调试模式的文字统一在第十层显示*/
		printDebugDataStr(opF, 0.00, endTime,  62, 35, NULL, "type");
		printDebugDataStr(opF, 0.00, endTime, 147, 35, NULL, "screen");
		printDebugDataStr(opF, 0.00, endTime, 232, 35, NULL, "blockCnt");
		printDebugDataStr(opF, 0.00, endTime, 317, 35, NULL, "count");
		printDebugDataStr(opF, 0.00, endTime, 402, 35, NULL, "total");
		#ifdef VERSION
		fprintf(opF, "\nDialogue:10,0:00:00.00,");
		printTime(opF, endTime, ",");
		fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\pos(35,245)\\b1\\fs25\\1c&HFFFFFF"
					 "\\1a&H35\\shad1\\bord0}DanmakuFactory v%s", VERSION);
		#endif
		
		/*显示弹幕类型表头，如果被屏蔽就使用划线*/
		if(blockMode[0] == '0' || blockMode[1] == '0' || blockMode[2] == '0' ||
		   blockMode[3] == '0' || blockMode[4] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 65, NULL, "ALL");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 65, "\\s1", "ALL");
		}
		if(blockMode[0] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 95, NULL, "R to L");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 95, "\\s1", "R to L");
		}
		if(blockMode[1] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 125, NULL, "L to R");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 125, "\\s1", "L to R");
		}
		if(blockMode[2] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 155, NULL, "TOP");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 155, "\\s1", "TOP");
		}
		if(blockMode[3] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 185, NULL, "BOTTOM");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 185, "\\s1", "BOTTOM");
		}
		if(blockMode[4] == '0')
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 215, NULL, "SPCIAL");
		}
		else
		{
			printDebugDataStr(opF, 0.00, endTime, 62, 215, "\\s1", "SPCIAL");
		}
		
		/*统一计算显示total的值*/
		for(cnt = 0; cnt < 6; cnt++)
		{
			printDebugDataInt(opF, 0.00, endTime, 402, 65 + cnt * 30, NULL, total[cnt]);
			if(total[cnt] == 0)
			{/*总数是0就开始统一显示0*/
				printDebugDataInt(opF, 0.00, endTime, 147, 65 + cnt * 30, NULL, 0);
				printDebugDataInt(opF, 0.00, endTime, 317, 65 + cnt * 30, NULL, 0);
				printDebugDataInt(opF, 0.00, endTime, 232, 65 + cnt * 30, NULL, 0);
			}
		}
		
		if(head -> time > TIME_EPS)
		{/*如果第一条弹幕不是从0.00开始就将全部数据写0 即head -> time > 0*/
			int i, j;
			for(i = 0; i < 3; i++)
			{
				for(j = 0; j <= NUM_OF_TYPE; j++)
				{
					printDebugDataInt(opF, 0.00, head -> time, 147 + i * 85, 65 + j * 30, NULL, 0);
				}
			}
		}
		fprintf(opF, "\n");
		
		/*遍历整个链表打印全部变化的数据*/
		signPtr = now = head;
		while(now != NULL)
		{
			/*移动指针到同屏第一条弹幕*/
			while(getEndTime(signPtr, rollTime, holdTime) < now -> time + TIME_EPS)
			{
				signPtr = signPtr -> next;
			}
			
			/*更新同屏弹幕计数器*/ 
			scanPtr = signPtr;
			arrset(screen, 0, NUM_OF_TYPE + 1);
			while(scanPtr != now -> next)
			{
				if(scanPtr -> type > 0 && getEndTime(scanPtr, rollTime, holdTime) > now -> time)
				{
					screen[scanPtr -> type]++;
				}
				scanPtr = scanPtr -> next;
			}
			
			/*当前弹幕加入弹幕总数计数器或屏蔽弹幕计数器*/
			if(now -> type > 0 && now -> type <= 5)
			{
				count[now -> type]++;
			}
			else if(now -> type < 0 && now -> type >= -5)
			{
				block[-(now -> type)]++;
			}
			
			
			/*开始更新表格数据*/
			if(now -> next != NULL)
			{/*起始寻找时间上限是下一条弹幕的开始*/
				endTime = (now -> next) -> time;
			}
			else
			{/*最后一次寻找时间上限是最后一条弹幕开始时间延迟30秒*/ 
				endTime = now -> time + 30;
			}
			
			if(fabs(now -> time - endTime) > TIME_EPS)
			{
				/*更新屏蔽弹幕计数器与弹幕总数计数器*/
				count[0] = count[1] + count[2] + count[3] + count[4] + count[5];
				block[0] = block[1] + block[2] + block[3] + block[4] + block[5];
				for(cnt = 0; cnt <= NUM_OF_TYPE; cnt++)
				{
					if(total[cnt])
					{
						printDebugDataInt(opF, now -> time, endTime, 232, 65 + 30 * cnt, NULL, block[cnt]);
						printDebugDataInt(opF, now -> time, endTime, 317, 65 + 30 * cnt, NULL, count[cnt]);
					}
				}
				
				/*更新同屏弹幕计数器 注*两条弹幕之前可能有弹幕退出屏幕*/
				int sameTimeNum[6] = {0};/*相同类型弹幕同时出现的次数计数器*/
				int minTimeType = 0;/*最小时间对应的弹幕类型*/
				float debugTempTime, debugMinTime;
				startTime = endTime = now -> time;
				while(TRUE)
				{/*在同屏弹幕中不断寻找结束时间的最小值，在这个时间点需要更新一条同屏弹幕-1的数据*/
					scanPtr = signPtr;
					arrset(sameTimeNum, 0, NUM_OF_TYPE + 1);
					if(now -> next == NULL)
					{/*起始寻找时间上限是下一条弹幕的开始*/
						debugMinTime = now -> time + 30;
					}
					else
					{/*最后一次寻找时间上限是最后一条弹幕开始时间延迟30秒*/
						debugMinTime = (now -> next) -> time;
					}
					while(scanPtr != now -> next)
					{/*循环寻找最小时间 用于作本条记录结束时间*/
						if((debugTempTime = getEndTime(scanPtr, rollTime, holdTime)) < TIME_EPS)
						{/*函数debugTempTime返回0.00表示弹幕发生了错误*/
							scanPtr = scanPtr -> next;
							continue;
						}
						
						if(debugTempTime > endTime && debugTempTime < debugMinTime &&
						   ((now->next == NULL && debugTempTime < now->time + 30) || 
						    (now->next != NULL && debugTempTime < (now->next) -> time)) )
						{/*debugMinTime > debugTempTime > endTime 且 debugTempTime在限定范围内*/
							debugMinTime = debugTempTime;
							
							/*最小值被刷新了，之前的相同类型弹幕统计也没有意义了*/
							minTimeType = scanPtr -> type;
							arrset(sameTimeNum, 0, NUM_OF_TYPE + 1);
						}
						if(debugMinTime == debugTempTime)
						{
							/*统计同一时间消失的各类型弹幕数量*/
							sameTimeNum[scanPtr -> type]++;
						}
						scanPtr = scanPtr -> next;
					}
					endTime = debugMinTime;
					if(startTime == endTime)
					{/*找不出一个最小时间 且 值未改变 遍历结束*/
						break;
					}
					
					screen[0] = screen[1] + screen[2] + screen[3] + screen[4] + screen[5];
					for(cnt = 0; cnt <= NUM_OF_TYPE; cnt++)
					{/*写数据*/
						if(total[cnt])
						{
							printDebugDataInt(opF, startTime, endTime,
											  147, 65 + 30 * cnt, NULL, screen[cnt]);
						}
					}
					startTime = endTime;/*以本条记录的结束时间作为下一条记录的开始时间*/
					for(cnt = 0; cnt <= NUM_OF_TYPE; cnt++)
					{/*更新同屏弹幕计数器 减去同一时间消失的弹幕*/ 
						screen[cnt] -= sameTimeNum[cnt];
					}
				}
			}
			
			now = now -> next;
		}/*结束 while*/

	}/*结束 if*/
	
	/*提醒图表即将关闭*/
	if(mode[0] != '0' || mode[1] != '0')
	{
		/*提示文字底框进场动画*/
		fprintf(opF, "\nDialogue: 9,");
		printTime(opF, lastDanmakuStartTime, ",");
		printTime(opF, lastDanmakuStartTime + 0.20, ",");
		/*底框*/
		if(mode[0] != '0')
		{
			fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\move(20,377,20,423)"
						 "\\iclip(m 20 340 l 445 340 l 445 385 l 20 385 l 20 340)\\1c&HEAF3CE\\1a&H35\\shad0"
						 "\\bord0\\p1}m 15 0 l 410 0 b 416 0 425 9 425 15 l 425 30 b 425 36 416 45 410 45"
						 " l 15 45 b 9 45 0 37 0 30 l 0 15 b 0 9 9 0 15 0{\\p0}");
		}
		else
		{
			fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\move(20,122,20,168)"
						 "\\iclip(m 20 85 l 445 85 l 445 130 l 20 130 l 20 85)\\1c&HEAF3CE\\1a&H35\\shad0"
						 "\\bord0\\p1}m 15 0 l 410 0 b 416 0 425 9 425 15 l 425 30 b 425 36 416 45 410 45"
						 " l 15 45 b 9 45 0 37 0 30 l 0 15 b 0 9 9 0 15 0{\\p0}");
		}
		
		
		
		/*提示文字底框固定后*/
		fprintf(opF, "\nDialogue: 9,");
		printTime(opF, lastDanmakuStartTime + 0.20, ",");
		printTime(opF, lastDanmakuStartTime + 30.00, ",");
		/*底框*/
		if(mode[0] != '0')
		{
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\1a&H35\\shad0\\bord0\\p1}"
						 "m 35 400 l 430 400 b 436 400 445 409 445 415 l 445 430 b 445 436 436 445 430 445"
						 " l 35 445 b 29 445 20 437 20 430 l 20 415 b 20 409 29 400 35 400{\\p0}");
		}
		else
		{
			fprintf(opF, "Default,,0000,0000,0000,,{\\an7\\1c&HEAF3CE\\1a&H35\\shad0\\bord0\\p1}"
						 "m 35 145 l 430 145 b 436 145 445 154 445 160 l 445 175 b 445 181 436 190 430 190"
						 " l 35 190 b 29 190 20 182 20 175 l 20 160 b 20 154 29 145 35 145{\\p0}");
		}
		
		/*刷新退出时间*/
		for(cnt = 0; cnt < 30; cnt++)
		{
			fprintf(opF, "\nDialogue: 9,");
			printTime(opF, lastDanmakuStartTime + cnt, ",");
			printTime(opF, lastDanmakuStartTime + cnt + 1.00, ",");
			/*底框*/
			if(mode[0] != '0')
			{
				fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\pos(35,422)\\b1\\fs25\\1c&HFFFFFF\\1a&H35"
							 "\\shad1\\bord0}These charts will close after %d s", 30 - cnt);				
			}
			else
			{
				fprintf(opF, "Default,,0000,0000,0000,,{\\an4\\pos(35,167)\\b1\\fs25\\1c&HFFFFFF\\1a&H35"
							 "\\shad1\\bord0}These charts will close after %d s", 30 - cnt);					
			}
		}
	}
	fflush(opF);
	fclose(opF);
	return 0;
}

/*
获取弹幕结束时间（秒）
参数： 
弹幕指针/滚动弹幕速度/固定弹幕保持时间 
返回值：
生存时间
错误返回0.00 
*/
float static getEndTime(DANMAKU *danmakuPtr, const int rollTime, const int holdTime)
{/*[0,0.17,"1-1",7,"文本部分内容",0,0,0,0.17,500,0,true,"微软雅黑",1]*/
	if(danmakuPtr == NULL)
	{
		return 0.00;
	}
	
	if(IS_R2L(danmakuPtr) || IS_L2R(danmakuPtr))
	{
		return danmakuPtr -> time + rollTime;
	}
	else if(IS_TOP(danmakuPtr) || IS_BTM(danmakuPtr))
	{
		return danmakuPtr -> time + holdTime;
	}
	else if(IS_SPECIAL(danmakuPtr))
	{/*特殊弹幕需要取持续时间*/ 
		char textTemp[64], *textPtr = danmakuPtr -> text;
		int cnt;
		for(cnt = 0; cnt < 3; cnt++)
		{/*直接跳过三个逗号*/
			while(*textPtr != ',')
			{
				textPtr++;
			}
			textPtr++;
		}
		return danmakuPtr -> time + strToFloat(deQuotMarks(strGetLeftPart(textTemp, &textPtr, ',')));
	}
	else
	{
		return 0.00;
	}
	
}

/*
以ass格式标准打印时间 
参数： 
文件指针/秒数/紧接着打印在后面的字符串
返回值：
ferror函数的返回值 
*/
int static printTime(FILE *filePtr, float time, char *endText)
{
	int hour = (int)time / 3600;
	time -= hour * 3600;
	int min = (int)time / 60;
	time -= min * 60;
	int sec = (int)time;
	int ms = (time - sec) * 100;
	fprintf(filePtr, "%01d:%02d:%02d.%02d%s", hour, min, sec, ms, endText);
	return ferror(filePtr);
}

/*
转换十进制rgb颜色为十六进制颜色
参数：
十进制颜色/记录十六进制的数组（7个字节以上） 
返回值：
十六进制数组的地址
*/ 
char static *toHexColor(int decColor, char *hexColor)
{/*先转换为16进制再两个为一对倒序排列*/
	int i, j;
	/*使用两个for实现位置交叉*/ 
	for(i = 0; i < 3; i++)
	{
		for(j = 1; j >= 0; j--)
		{
			switch(decColor % 16)
			{
				default:
				{
					hexColor[2 * i + j] = decColor % 16 + '0';
					break;
				}
				case 10:
				{
					hexColor[2 * i + j] = 'A';
					break;
				}
				case 11:
				{
					hexColor[2 * i + j] = 'B';
					break;
				}
				case 12:
				{
					hexColor[2 * i + j] = 'C';
					break;
				}
				case 13:
				{
					hexColor[2 * i + j] = 'D';
					break;
				}
				case 14:
				{
					hexColor[2 * i + j] = 'E';
					break;
				}
				case 15:
				{
					hexColor[2 * i + j] = 'F';
					break;
				}
			}
			decColor /= 16;
		}
	}
	hexColor[6] = '\0'; 
	return hexColor;
}

/*
十进制透明度转十六进制透明度（0-255） 
*/
char static *toHexOpacity(int decOpacity, char *hexOpacity)
{
	int cnt;
	for(cnt = 1; cnt >=0; cnt--)
	{
		switch(decOpacity % 16)
		{
			default:
			{
				hexOpacity[cnt] = decOpacity % 16 + '0';
				break;
			}
			case 10:
			{
				hexOpacity[cnt] = 'A';
				break;
			}
			case 11:
			{
				hexOpacity[cnt] = 'B';
				break;
			}
			case 12:
			{
				hexOpacity[cnt] = 'C';
				break;
			}
			case 13:
			{
				hexOpacity[cnt] = 'D';
				break;
			}
			case 14:
			{
				hexOpacity[cnt] = 'E';
				break;
			}
			case 15:
			{
				hexOpacity[cnt] = 'F';
				break;
			}
		}
		decOpacity /= 16;
	}
	hexOpacity[2] = '\0';
	return hexOpacity;
}

/*
寻找最小值 
参数：
欲找最小值的数组/成员数/终止下标/模式（0正序，1逆序）
返回值：
最小值数组下标 
*/
int static findMin(float *array, const int numOfLine, const int stopSubScript, const int mode)
{
	int cnt, minSub;
	if(!mode)
	{	/*正序排序*/
		minSub = 0;
		for(cnt = 0; cnt < stopSubScript; cnt++)
		{
			if(array[cnt] < array[minSub])
			{
				minSub = cnt;
			}
		}
	}
	else
	{	/*逆序排序*/ 
		minSub = numOfLine - 1;
		for(cnt = numOfLine - 1; cnt >= stopSubScript; cnt--)
		{
			if(array[cnt] < array[minSub])
			{
				minSub = cnt;
			}
		}
	}
	return minSub;
}


/*
根据信息打印一个ass矩形（绘图部分）
参数：
文件指针/起点X坐标/起点Y坐标/宽度/高度
返回值：
ferror函数返回值 
*/
int static drawRectangle(FILE *opF, int startX, int startY, int width, int height)
{
	fprintf(opF, "m %d %d l %d %d l %d %d l %d %d l %d %d",
				startX, startY, startX + width, startY,
				startX + width, startY + height, startX, startY + height,
				startX, startY);
	return ferror(opF);
}


/*
根据信息打印debug数据表上的信息（整数） 
参数： 
文件指针/开始秒数/结束秒数/强制定位x/强制定位Y/特效追加/整型数据
返回值：
ferror函数的返回值 
*/
int static printDebugDataInt(FILE *filePtr, const float startTime, const float endTime, const int posX,
								const int posY, char *effect, const int data)
{
	fprintf(filePtr, "\nDialogue:10,");
	printTime(filePtr, startTime, ","); 
	printTime(filePtr, endTime, ",");
	fprintf(filePtr, "Default,,0000,0000,0000,,{\\an5\\pos(%d,%d)\\b1", posX, posY);
	if(effect != NULL)
	{
		fprintf(filePtr, "%s", effect);
	}
	fprintf(filePtr, "\\fs25\\1c&HFFFFFF\\1a&H35\\shad1\\bord0}%d", data);
	return ferror(filePtr);
}

/*
根据信息打印debug数据表上的信息（字符串） 
参数： 
文件指针/开始秒数/结束秒数/强制定位x/强制定位Y/特效追加/字符串 
返回值：
ferror函数的返回值 
*/
int static printDebugDataStr(FILE *filePtr, const float startTime, const float endTime, const int posX,
						const int posY, char *effect, char *str)
{
	fprintf(filePtr, "\nDialogue:10,");
	printTime(filePtr, startTime, ","); 
	printTime(filePtr, endTime, ",");
	fprintf(filePtr, "Default,,0000,0000,0000,,{\\an5\\pos(%d,%d)\\b1", posX, posY);
	if(effect != NULL)
	{
		fprintf(filePtr, "%s", effect);
	}
	fprintf(filePtr, "\\fs25\\1c&HFFFFFF\\1a&H35\\shad1\\bord0}%s", str);
	return ferror(filePtr);
}

/*
数组清空 
参数： 
数组首地址/要初始化成的值/成员数量/ 
返回值：
ferror函数的返回值 
*/
void static arrset(int *array, const int value, const int numberOfMember)
{
	int cnt;
	for(cnt = 0; cnt < numberOfMember; cnt++)
	{
		*(array + cnt) = value;
	}
	return;
}
