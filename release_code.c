#include <stdio.h>  
#include <string.h> 
#include <stdlib.h>
#include <sys/stat.h>  

static char *g_pszConfigs = NULL;
static char *g_pszUnConfigs = NULL;
#define MAX_STATE_NUM 10
static char g_linestate[MAX_STATE_NUM]; /* 多个嵌套 */



enum linestate
{
	STATE_NORMAL,
	STATE_NOTE,
	STATE_MACRO_FILTER,
	STATE_MACRO_NOMARL
};

void getConfigs(char* filename)  
{
	int size;
    struct stat statbuf;  
    FILE * fp;
    char acBufIn[128] = {0};	
	char *pszConfigs = NULL;
	char *pszUnConfigs = NULL;
	
    stat(filename,&statbuf);  
    size = statbuf.st_size;  
	g_pszConfigs = (char *)malloc(size);
	if(NULL == g_pszConfigs)
	{
		return;
	}
	g_pszUnConfigs = (char *)malloc(size);
	if(NULL == g_pszUnConfigs)
	{
		free(g_pszConfigs);
		g_pszConfigs = NULL;
		return;
	}
	
	fp = fopen(filename, "rt");
    if (NULL == fp)
    {
        return;
    }
	
	memset(g_pszConfigs, 0, size);
	memset(g_pszUnConfigs, 0, size);

	pszConfigs = g_pszConfigs;
	pszUnConfigs = g_pszUnConfigs;
	
    while (fgets(acBufIn, sizeof(acBufIn), fp) != NULL)
    {
        if ((strstr(acBufIn, "=y")) || (strstr(acBufIn, "=Y")))
        {
             pszConfigs += sprintf(pszConfigs, "%s\n", acBufIn);
        }
        else if (strstr(acBufIn, "is not set"))
        {
            pszUnConfigs += sprintf(pszUnConfigs, "%s\n", acBufIn);
        }
        memset(acBufIn, 0, sizeof(acBufIn));
    }
    fclose(fp);
    return ;  	
}

int checkConfigExist(char *pConfig)
{
	if(NULL == pConfig)
	{
		return 0;
	}
	
	if(strstr(g_pszConfigs, pConfig))
	{
		return 1;
	}
	
	return 0;
}

int checkUnConfigExist(char *pConfig)
{
	if(NULL == pConfig)
	{
		return 0;
	}
	
	if(strstr(g_pszUnConfigs, pConfig))
	{
		return 1;
	}
	
	return 0;	
}

int checkConfigValue(char *pLine) /* 暂时支持简单表达式 defined（） && defined（） || defined（） */
{
	int ret = 0;
	char *p = NULL;
	char *pConfig = NULL;
	int prefixOpt = 1; /* || or &&  */
	int checkConfig;
	
	if(NULL == pLine)
	{
		return 1;
	}
	
	p = pLine;
	
	while(NULL != p)
	{
		pConfig = strstr(p, "CONFIG_"); /* 限制是CONFIG_前缀 */
		
		if(NULL == pConfig)
		{
			return 1;
		}
		
		p = strstr(pConfig, ")");
		if(p == pConfig)
		{
			return 1;
		}
		
		*p = 0;
		
		checkConfig = checkConfigExist(pConfig);
		if(prefixOpt)
		{
			ret = (ret || checkConfig);
		}
		else
		{
			ret = (ret && checkConfig);
		}
		
		p++;	
		
		if(strstr(p, "||"))
		{
			prefixOpt = 1;
		}
		else if(strstr(p, "&&"))
		{
			prefixOpt = 0;
		}
		else
		{
			break;
		}
		
	}	
	
	return ret;
}

int lineStateGet(void)
{
	int i = 0;
	int lastState = g_linestate[i];
	
	for(i = 0; i < MAX_STATE_NUM; i++)
	{
		if(STATE_NORMAL == g_linestate[i])
		{
			break;
		}
		else
		{
			lastState = g_linestate[i];
		}
	}
	
	return lastState;
}

void lineStateSet(int state)
{
	int i = 0;
	int lastStateIdx = i;
	
	for(i = 0; i < MAX_STATE_NUM; i++)
	{
		if(STATE_NORMAL == g_linestate[i])
		{
			break;
		}
		else
		{
			lastStateIdx = i;
		}
	}	
	
	if(STATE_NORMAL == state)
	{
		g_linestate[lastStateIdx] = state;
	}
	else
	{
		if(i < MAX_STATE_NUM)
		{
			g_linestate[i] = state;
		}
	}
	
	
}

int lineStateNormalHandle(char *pLine)
{
	char *p = NULL;
	char *pConfig = NULL;
	char *pConfigEnd = NULL;
	
	if(NULL == pLine)
	{
		return 1;
	}	
	/* 暂不考虑#或/×前面有代码的情况 */
	if('#' != p[0]) 
	{
		if(('/' != p[0]) && ('*' != p[1]))
		{
			lineStateSet(STATE_NOTE);
		}
		
		return 0;
	}
	
	if(strstr(p, "include"))
	{
		return 0;
	}
	
	if(strstr(p, "ifdef"))
	{
		pConfig = strstr(p, "CONFIG_"); /* 限制是CONFIG_前缀 */
		
		if(NULL == pConfig)
		{
			return 0;
		}
		
		/* 暂只通过CONFIG_宏来裁剪代码 */
		if(checkUnConfigExist(pConfig))
		{
			lineStateSet(STATE_MACRO_FILTER);
		}
		
		if(checkConfigExist(pConfig))
		{
			lineStateSet(STATE_MACRO_NOMARL);
		}
		return 1;
	}
	else if(strstr(p, "if"))
	{
		if(checkConfigValue(p))
		{
			lineStateSet(STATE_MACRO_NOMARL);
		}
		else
		{
			lineStateSet(STATE_MACRO_FILTER);
		}
		return 1;
	}	
	
	return 0;
}

int lineStateNoteHandle(char *pLine)
{
	char *p = NULL;
	
	if(NULL == pLine)
	{
		return 1;
	}	
	/* 暂不考虑#或/×前面有代码的情况 */
	if(('*' != p[0]) && ('/' != p[1]))
	{
		lineStateSet(STATE_NORMAL);
		return 0;
	}
	
	if(strstr(p, "*/"))
	{
		lineStateSet(STATE_NORMAL);
		return 0;
	}
	
	return 0;
}

int lineStateMacroFilterHandle(char *pLine)
{
	char *p = NULL;
	
	if(NULL == pLine)
	{
		return 1;
	}	
	/* 暂不考虑#或/×前面有代码的情况 */
	if('#' != p[0]) 
	{	
		return 1;
	}
	
	if(strstr(p, "endif"))
	{
		lineStateSet(STATE_NORMAL);
		return 1;
	}
	
	if(strstr(p, "else"))
	{
		lineStateSet(STATE_NORMAL);
		lineStateSet(STATE_MACRO_NOMARL);
		return 1;
	}
	
	if(strstr(p, "elif"))
	{
		lineStateSet(STATE_NORMAL);
		if(checkConfigValue(p))
		{
			lineStateSet(STATE_MACRO_NOMARL);
		}
		else
		{
			lineStateSet(STATE_MACRO_FILTER);
		}
		return 1;
	}
	
	return 1;
}

int lineStateMacroNormalHandle(char *pLine)
{
	char *p = NULL;
	
	if(NULL == pLine)
	{
		return 1;
	}	
	/* 暂不考虑#或/×前面有代码的情况 */
	if('#' != p[0]) 
	{	
		return 0;
	}
	
	if(strstr(p, "endif"))
	{
		lineStateSet(STATE_NORMAL);
		return 1;
	}
	
	if(strstr(p, "else"))
	{
		lineStateSet(STATE_NORMAL);
		lineStateSet(STATE_MACRO_FILTER);
		return 1;
	}
	
	if(strstr(p, "elif"))
	{
		lineStateSet(STATE_NORMAL);
		lineStateSet(STATE_MACRO_FILTER);
		return 1;
	}
	
	return 0;
}

int checkLineFilter(char *pLine)
{
	char *p = NULL;
	char *pConfig = NULL;
	char *pConfigEnd = NULL;
	int lineState;
	
	if(NULL == pLine)
	{
		return 1;
	}
	
	p = pLine;
	
	while(NULL != p && *p == ' ')
	{
		p++;
	}
	
	lineState = lineStateGet();
	
	switch (lineState)
	{
		case STATE_NORMAL:
			return lineStateNormalHandle(p);
		case STATE_NOTE:
			return lineStateNoteHandle(p);
		case STATE_MACRO_FILTER:
			return lineStateMacroFilterHandle(p);
		case STATE_MACRO_NOMARL:
			return lineStateMacroNormalHandle(p);
		default:
			break;
	}
	
	return 0;
}

int main(int argc, char*argv[])
{
	int i;
	char *szFileName = NULL;
	char *szConfigFile = NULL;
	char szTmpFileName[128]={0};
	char szCmd[128]={0};
	FILE	*pFileIn = NULL;
	FILE	*pFileOut = NULL;
    char	szLine[512]; /* each line len 512  */
	
	#if 1
	for (i=0; i<argc; i++) {
         printf("argv i %d %s", i, argv[i]);            
    }
	#endif
	
	szConfigFile =  argv[1];
	szFileName =  argv[2];
	
	sprintf(szTmpFileName, "%s.bak", szFileName);
	sprintf(szCmd, "mv %s %s", szFileName, szTmpFileName);
	system(szCmd);
	
	getConfigs(szConfigFile);
	
	pFileIn = fopen(szTmpFileName, "r");
	
	if (NULL  == pFileIn) 
	{
        printf("could not open %s\n", szTmpFileName);
        return 0;
    }
	
	pFileOut = fopen(szFileName, "w");
	if (NULL  == pFileOut) 
	{
        printf("could not open %s\n", szFileName);
        return 0;
    }
	
	while (fgets(szLine, sizeof(szLine), pFileIn)) 
	{
        if(0 == checkLineFilter(szLine))
		{
			fwrite(szLine, strlen(szLine), sizeof(char), pFileOut);
		}
    }

    fclose(pFileIn);
	fclose(pFileOut);
	
	free(g_pszConfigs);
	g_pszConfigs = NULL;
	
	return 0;
}
