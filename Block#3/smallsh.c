#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


void cdCommand();
void getInput();
void exitCommand();
void checkBuiltIn(struct sigaction SIGINT_action);
void token();
void forkFork(struct sigaction SIGINT_action);
void bgCheck();
void redirectionCheck();
void cashToCoinsV1();
void reapEm();
void catchSIGTSTP(int signo);
void catchSIGIGN(int signo);
void statusView();

#define MAX_BUFFER 2048

char home[1024];

char* userInput = NULL;

size_t input = 2048;

int argc;

char* argv[512];

char test[50];

int status;

int backGroundCommand = 0;

int backGroundEnabled = 1;

int bgPID[2048] = {0};

int bgPIDcount = 0;




int main()
{

	struct sigaction SIGTSTP_action = {0};	//struct for my ctrl-Z signal, this allows me to catch the signal and call my "catchSIGTSTP" handler to toggle background and foreground
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	sigaction(SIGTSTP,&SIGTSTP_action, NULL);

	struct sigaction SIGINT_action = {0};	//struct for my ctrl-C signal, this allows me to catch the signal and IGN and its default is to terminate the process!
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	sigaction(SIGINT,&SIGINT_action,NULL);


	while(1)	//main loop that continues to get user input, validate,store and then fork/exec when needed. Loop while true so user may continue using smallshell until exit
	{	reapEm();
		getInput();
		token();
		cashToCoinsV1();
		bgCheck();
		checkBuiltIn(SIGINT_action);
	}
	return 0;
}


//1 = foreground and background, -1 = foreground only
void catchSIGTSTP(int signo)	//handler function for ctrl-z that will toggle between foreground only or background/foreground.
{
	char* infoToUser = "\nEntering foreground-only mode (& is now ignored)\n";	//notitfy user that the signal has been received and changing mode
	char* infoToUserExit = "\nExiting foreground-only mode (& is now enabled)\n";
	if(backGroundEnabled == 1)	//check if background has been turned off
	{
		write(STDOUT_FILENO,infoToUser,51);
		fflush(stdout);
	}
	else if(backGroundEnabled == -1)	//check if background has been turned on
	{

		write(STDOUT_FILENO,infoToUserExit,50);
		fflush(stdout);
	}



	backGroundEnabled = backGroundEnabled * -1; //my system to flip back and forth after every signal is called, simplier than making a check.
}



void getInput()	//function that will get user input via getline() I.E grabs the whole line as raw data
{	int charLength;
	while(1)
	{	printf(":");
		fflush(stdout);

		charLength = getline(&userInput,&input,stdin);	//grab the input from user and store the amount of bytes read in
		if(charLength == -1)	//ensure its healthy
		{
			clearerr(stdin);
		}
		else
		{
			if((userInput[charLength - 1] == '\n'))	//check and then remove trailing newline and add the end of a string a.k.a, null terminator
			{
				userInput[charLength -1] = '\0';
			}

			fflush(stdin);
			if(userInput[0] == '\0' || userInput[0]== '#')	//simple check to see if first argv is empty(we know its a blank line or check if its a commentline, ignore
			{
				getInput();	//prompt user for more data as the line they entered was irrelevant
			}
			break;
		}
	}

}

void cashToCoinsV1()	//function that will convert money money to pID of process
{
	pid_t pID;
	int x;
	int cashFound;
	char holdArg[1024];
	char charPID[1024];


	memset(charPID,'\0',1024); //lets be safe and set the mem to null so we have an end to the string
	pID = getpid();	//get the pID so we can transfer it to the locatio of money money
	sprintf(charPID,"%d",pID);

	for(x = 0;x < argc; x++)	//for every argv, check to see if there is $$ found
	{	memset(holdArg,'\0',1024);

		strcpy(holdArg,argv[x]);	//copy contet of argv inro dummy array

		if(strstr(holdArg,"$$") != '\0')	//search for $$ in string
		{
			memcpy(holdArg,argv[x],strlen(argv[x]) -2);
			holdArg[strlen(argv[x])-2] = '\0';
			strcat(holdArg,charPID);	//actually store the string pID into the dummy string then transplant that back into real argv for user
			strcpy(argv[x],holdArg);
		}

	}

}


void token()	//function that will allow us to tokenize the whole line obtained from getInput()
{ char* toke;
  int len;
  int i =0;
  int x;
 	argc = 0;
  	for(x=0;x<512;x++)	//lets make sure every index is null, this also acts as a argv reset so we can take more args each iteration
	{
		argv[x] = NULL;
	}
	toke = strtok(userInput," "); //set delimeter to " " so we may grab each argument and store it in toke string
	while(toke != NULL)
	{
		len = strlen(toke);
		argv[i]=malloc(sizeof(char)*len);
		memset(argv[i],'\0',len);
		argv[i] = toke;		//store each arg in the argv array then incrememnt the count of args so we know how many user gave us
	        toke = strtok(NULL," ");
		argc++;
		i++;
	}

}

void bgCheck()	//function that will check and see if the process is meant to be ran in the background
{
	if(strcmp(argv[argc-1],"&") == 0 && backGroundEnabled == 1)
	{

		backGroundCommand = 1;	//if we found an & and we are in background Mode, lets toggle that it will be a background process via setting int to 1(true)
		fflush(stdout);
		argv[argc-1] = '\0';	//remove the & from argv array so exec doesnt try to run as argument


	}
	else if(strcmp(argv[argc-1],"&") == 0 && backGroundEnabled == -1)	//if its not meant to be in the background then lets handle it
	{
		backGroundCommand = 0;	//toggle off background mode so it will be ran in foreground
		argv[argc-1] = '\0';
	}
	else
	{
		backGroundCommand = 0;
	}
}

void reapEm()	//function that does the reapping of bad kids
{
	int x;
	int childStat = -2;
	int childSIG = -2;

	for(x = 0; x <bgPIDcount; x++)	//for as many bad kids as we have, check them all
	{
		if(bgPID[x] > 0)	//make sure its an actual pID
		{
			if(waitpid(bgPID[x],&status,WNOHANG))	//reap them
			{
				if(WIFEXITED(status) != 0)	//lets see if the exited normally
				{	childStat = WEXITSTATUS(status);	//lets grab the actual status so we can inform user
					printf("Background pid %d done, exit status was: %d\n",bgPID[x],childStat);
					fflush(stdout);
					//status = childStat;
				}
				else
				{
					childSIG = WTERMSIG(status);	//lets see what signal ended the process
					printf("Background pid %d finished, signal was %d\n",bgPID[x],childSIG);
					fflush(stdout);
					//status = childSIG;
				}
				bgPID[x] = 0; // "remove" bad kid from array of pID's
			}

		}
	}
}

void redirectionCheck()	//function that deals with redirection
{	int fdWrite;
	int fdRead;
	char* noTargetBG = "/dev/null";
	int x;
	char* space = '\0';


	for(x = 0; x < argc; x++)	//lets check all the args
	{
		if(argv[x] != NULL)	//make sure we arent comparing a string to NULL so we dont cause problems
		{
			if(backGroundCommand == 1)	//if the process is going to be background we need to redirect to dev/null, unless they specify otherwise
			{
				fdWrite = open(noTargetBG,O_RDWR | O_TRUNC | O_CREAT, 0600);	// redirect stdout of background process to dev/null so it doesnt reach terminal
				dup2(fdWrite,1);

				fdRead = open(noTargetBG,O_RDONLY);	//redirect stdin of background process so its reads from dev/null
				dup2(fdRead,0);

			}
			if(strcmp(argv[x], ">") == 0)	//if the user has specified that there needs to be redirection
			{
				argv[x] = space;	//remove the '>' from the argv array so no issues are encountered with exec()

				if(backGroundCommand == 1)	//if the process is meant to be in the background and they have gave us a file to redirect to, lets change destination
				{
					if(argv[x+1] != NULL)
					{
						fdWrite = open(argv[x+1],O_RDWR | O_TRUNC | O_CREAT, 0600);
					}
					fdWrite = open(noTargetBG,O_RDWR | O_TRUNC | O_CREAT, 0600);
					argv[x+1] = space;
					if(fdWrite == -1)	//make sure the file was able to be opened
					{
						perror("Error opening dev/null for writing..\n");
						exit(1);
					}
					else
					{
						dup2(fdWrite,1);	//perform redirection
					}
				}
				else
				{
					fdWrite = open(argv[x+1],O_RDWR | O_TRUNC | O_CREAT, 0600);	//if not a background process, do redirection to file
					argv[x+1] = space;

					if(fdWrite == -1)	//check that the file was able to be opened
					{
						perror("Error opening for writing...\n");
						exit(1);
					}

					dup2(fdWrite,1);	//perform redirection
					close(fdWrite);
				}



			}
			else if(strcmp(argv[x], "<") == 0)	//redirection is with read and not write this time
			{
				argv[x] =space;	//same as above, remove it

				if(backGroundCommand == 1)	//if its background and they have specified a file to read from, read it!
				{
					if(argv[x+1] != NULL)	//make sure its a valid name
					{
						fdRead = open(argv[x+1],O_RDONLY);
					}
					fdRead = open(noTargetBG,O_RDONLY);
					argv[x+1] = space;
					if(fdRead == -1)	//ensure it was able to be opened for reading
					{
						perror("Error opening dev/null for writing..\n");
						exit(1);
					}
					else
					{
						dup2(fdRead,1);	//redirec the stdin
					}
				}
				else	//if its not a background process then redirect as normal
				{

					fdRead = open(argv[x+1],O_RDONLY);
					argv[x+1] = space;
					if(fdRead == -1)	//ensure it was able to open properly for reading
					{
						perror("Cannot open, badfile for input");
						exit(1);
					}
					dup2(fdRead,0);	//redirect
					close(fdRead);
				}
			}
		}
	}

}


void cdCommand()	//function that handles moving dirs
{
	if(argv[1] == NULL)	//if arg after "cd" is empty, we need to go home
	{
		chdir(getenv("HOME"));
	}
	else
	{
		chdir(argv[1]);	//else move to the specified dir
	}
	fflush(stdout);
}

void exitCommand()	//lessa get outta here bois!
{
	exit(0);
}

void forkFork(struct sigaction SIGINT_action )	//function to fork off from parent
{
	pid_t spawnChild = -2;
	int childExitMethod = -2;
	int resultExec;

	spawnChild = fork();		//fork off from parent
	if(spawnChild == -1)	//check that we had a successful fork()
	{
		perror("Error...Error...Error");
		status = 1;
		exit(1);
	}
	else if(spawnChild == 0) // child was created successfully usig fork()
	{	if(backGroundCommand == 0)	//check if its meant for background, if so we need to enabl def handler for SIGINT so it will be killed by signal and not parent
		{
			fflush(stdout);
			SIGINT_action.sa_handler = SIG_DFL;
			sigaction(SIGINT,&SIGINT_action,NULL);
		}

		redirectionCheck();	//check for redirection

		resultExec = execvp(argv[0],argv);	//exec from fork and run new program as it isnt a built - in
		if( resultExec != 0)
		{
			perror("Exec failed ");
			status = 1;
			exit(1);
		}

	}
	else	//we are checking the parent process here
	{
		if(backGroundCommand == 0)	//if the parent is the in background we need to wait to return command to user, aka, we dont use WNOHANG flag
		{	do{
			waitpid(spawnChild,&status,0);
			}while(!WIFEXITED(status) && !WIFSIGNALED(status)); // check to see if the process has terminated

			if(WIFSIGNALED(status) != 0)
			{	printf(" terminated by signal 2\n"); // let user know that it was terminated by SIG
				fflush(stdout);

			}
			if(WIFEXITED(status) != 0)
			{
				fflush(stdout);

			}

		}
		if(backGroundCommand == 1)	//if the parent is backgrounded we need to inform user of pID
		{
			printf("background pid is %d\n",spawnChild);
			fflush(stdout);
			backGroundCommand = 0;
			bgPID[bgPIDcount] = spawnChild;	//add bad kid to array of pID's
			bgPIDcount++;

			 waitpid(spawnChild,&status,WNOHANG);	//do not block parent as its meant to run in background

			fflush(stdout);

		}


	}



}

void statusView()	//function that will display the status of the last terminated process
{
	if(WIFEXITED(status))	//check if it terminated normally and has a status
	{
		printf("exit value %d \n", WEXITSTATUS(status));
		fflush(stdout);
	}
	else
	{
		printf("terminated by signal %d \n", status); //process was killed by signal, lets find out which one
		fflush(stdout);
	}


}

void checkBuiltIn(struct sigaction SIGINT_action)	//function that is basically a switch for the smallsh's built- ins
{

	if(strcmp(argv[0],"cd") == 0)
	{
		cdCommand();
	}
	else if(strcmp(argv[0],"exit") == 0)
	{
		exitCommand();
	}
	else if(strcmp(argv[0],"status") == 0)
	{
		statusView();
	}
	else
	{
		forkFork(SIGINT_action);	//if not a built in, we fork outta here!
	}

}

