/**************************************************************************
 * Author: 	    Ernest Kim
 * Date: 	    2/12/2020
 * Description: Small shell programming assignment 3
**************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define DEFAULT -5;

//Global for catching signals for child processes
pid_t spawnPID = DEFAULT;
pid_t backPID = DEFAULT;
bool foregroundOnly = false;
int signalNumber = DEFAULT;
bool signalCaught = false;
int childExitMethod = DEFAULT;
const char homePath[] = "/nfs/stak/users/kimer";

//Action for CTRL-C
void catchSIGINT (int signo) {
	//Output message to console
	write(STDOUT_FILENO, "terminated by signal ", strlen("terminated by signal "));
	char sigNum[2];
	memset(sigNum, '\0', 2);
	sprintf(sigNum, "%d", signo);
	write(STDOUT_FILENO, sigNum, strlen(sigNum));
	char * newLine = "\n";
	write(STDOUT_FILENO, newLine, 1);

	//Kill process
	signalNumber = signo;
	signalCaught = true;
	kill(spawnPID, SIGTERM);
}

//Action for CTRL-Z
void catchSIGTSTP (int signo) {
	//Switch between foreground modes
	if (!foregroundOnly) {
		write(STDOUT_FILENO, "Entering foreground-only mode (& is now ignored)\n", strlen("Entering foreground-only mode (& is now ignored)\n"));
		foregroundOnly = true;
	} else if (foregroundOnly) {
		write(STDOUT_FILENO, "Exiting foreground-only mode\n", strlen("Exiting foreground-only mode\n"));
		foregroundOnly = false;
	}

	write(STDOUT_FILENO, ": ", strlen(": "));
}

//Every time a child process terminates
void catchSIGCHLD (int signo) {
	int childPID = wait(&childExitMethod);
	int childPIDCopy = childPID;
	int pidDigit = 0;

	do {
		pidDigit++
		childPIDCopy /= 10;
	} while(childPIDCopy != 0);

	char pidNum[pidDigit];
	memset(pidNum, '\0', sizeof(pidNum));
	sprintf(pidNum, "%d", childPID);

	write(STDOUT_FILENO, "background pid ", strlen("background pid "));
	write(STDOUT_FILENO, pidNum, pidDigit);
	write(STDOUT_FILENO, " is done: exit value 1\n", strlen(" is done: exit value 1\n"));
}

int main (int argc, char * argv[]) {
	int numCharsEntered = DEFAULT;
	size_t bufferSize = 2048;
	char * command = NULL;
	pid_t childPID;

	//Signal catchers
	struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, SIGCHLD_action = {0}, ignore_action = {0};

	//SIGINT
	SIGINT_action.sa_handler = catchSIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	//SIGTSTP
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	//SIGCHLD
	SIGCHLD_action.sa_handler = catchSIGCHLD;
	sigfillset(&SIGCHLD_action.sa_mask);
	SIGCHLD_action.sa_flags = 0;

	//Print initial prompt
	printf(": ");
	fflush(stdout);

	//Enter shell
	while(true) {
		//Grab input
		numCharsEntered = getline(&command, &bufferSize, stdin);
		if (numCharsEntered == -1)
			clearerr(stdin);
		command[strcspn(command, "\n")] = '\0'; //Get rid of newline char that getline adds

		///////////////////////////STATUS COMMAND///////////////////////////////////
		if ((strcmp(command, "status") == 0) || (strcmp(command, "status &") == 0)) {
			wait(&childExitMethod);

			//Error
			if (spawnPID == -1) {
				perror("wait failed");
				exit(1);
			}

			//Normal termination
			if ((WIFEXITED(childExitMethod)) && (!signalCaught)) {
				int exitStatus = WEXITSTATUS(childExitMethod);
				printf("exit value %d\n", exitStatus);
				fflush(stdout);
			} 

			//Terminated by signal
			if ((WIFSIGNALED(childExitMethod)) || (signalCaught)){
				printf("terminated by signal %d\n", signalNumber);
				fflush(stdout);
				signalCaught = false;
			}
		
			//Print prompt
			printf(": ");
			fflush(stdout);
		}

		/////////////////////////////CD COMMAND///////////////////////////////////////
		//cd with no arguments after
		else if ((command[0] == 'c') && (command[1] == 'd') && (command[2] == '\0')) {
			chdir(homePath);
			printf(": ");
			fflush(stdout);
		}

		//Normal cd command with argument after
		else if ((command[0] == 'c') && (command[1] == 'd') && (command[2] == ' ')) {
			//Move pointer to given path name
			command += 3;
			
			char pathName[50];
			memset(pathName, '\0', sizeof(pathName));
			strcpy(pathName, command);

			//Detect $$ when it's part of argument
			for (int k = 0; k < sizeof(pathName); k++){
				if((pathName[k] == '$') && (pathName[k + 1] == '$')){
					int childPID = getpid();
					int childPIDCopy = childPID;
					int pidDigit = 0;

					do {
						pidDigit++
						childPIDCopy /= 10;
					} while(childPIDCopy != 0);

					char pidNum[pidDigit];
					memset(pidNum, '\0', sizeof(pidNum));
					sprintf(pidNum, "%d", childPID);

					//Append pidNum to the pathname
					for (int p = 0; p < pidDigit; p++){
						pathName[k + p] = pidNum[p];
					}
				}
			}

			//Change directory and print prompt
			chdir(pathName);
			printf(": ");
			fflush(stdout);
		}

		/////////////////////////////////EXIT COMMAND////////////////////////////////
		else if ((strcmp(command, "exit") == 0) || (strcmp(command, "exit &") == 0)) {
			return 0;
		}

		/////////////////////////////////////BASH COMMANDS////////////////////////////
		else {
			//Build argument array for execvp
			char * holder;
			char * argument[512];
			holder = strtok(command," ");
			int j = 0;

			//Grab arguments
			while (holder != NULL) {
				argument[j] = holder;
				holder = strtok(NULL, " ");
				j++;
			}

			//Last argument
			argument[j] = NULL;

			//Control variables
			bool goFork = true;
			bool redTarget = false;
			bool redSource = false;
			int redirect, redirect2, targetFD, sourceFD;

			//$$ command to expand PID of shell
			for (int i = 0; i < j; i++) {
				char temp[50];
				memset(temp, '\0', sizeof(temp));
				strcpy(temp, argument[i]);

				for (int k = 0; k < sizeof(temp); k++){
					if((temp[k] == '$') && (temp[k + 1] == '$')){
						int childPID = getpid();
						int childPIDCopy = childPID;
						int pidDigit = 0;

						do {
							pidDigit++
							childPIDCopy /= 10;
						} while(childPIDCopy != 0);

						char pidNum[pidDigit];
						memset(pidNum, '\0', sizeof(pidNum));
						sprintf(pidNum, "%d", childPID);

						for (int p = 0; p < pidDigit; p++){
							temp[k + p] = pidNum[p];
						}

						argument[i] = temp;
					}

				}
			}

			//Detect redirection or process ID
			for (int i = j; i > 0; i--) {
				//Detect redirect output
				if (strcmp(argument[i], ">") == 0) {
					targetFD = open(argument[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
					redTarget = true;
					argument[i] = NULL;
					argument[i + 1] = NULL;
					j = i;
				} 

				//Detect redirect input
				if (strcmp(argument[i], "<") == 0) {
					sourceFD = open(argument[i + 1], O_RDONLY);

					if (sourceFD == -1) {
						printf("cannot open %s for input\n", argument[i +1]);
						fflush(stdout);
						goFork = false;
						childExitMethod = 1;
					} else {
						redSource = true;
						argument[i] = NULL;
						argument[i + 1] = NULL;
						j = i;
					}
				}
	
				//Detect process ID
				if (strcmp(argument[i], "$$") == 0) {
					char pidString[20];
					memset(pidString, '\0', sizeof(pidString));
					int pidInt = getpid();
					sprintf(pidString, "%d", pidInt);
					argument[i] = pidString;
				}
			}

			//Run command
			if (goFork) {
				//Background process
				if ((strcmp(argument[j - 1], "&") == 0) && (!foregroundOnly)) {
					argument[j - 1] = NULL;
					printf("background pid is ");
					fflush(stdout);
					int parentPID = getpid();

					//Fork
					spawnPID = fork();

					//Print child PID of background
					if (spawnPID == 0) {
						printf("%d\n", getpid());
						fflush(stdout);
					}

					//Follow child process
					switch (spawnPID) {
						//Error
						case -1: { 
							perror("ERROR IN FORKING PROCESS\n");
							fflush(stderr);
							exit(1);
							break;
						}

						//Success
						case 0: {
							//Redirection
							if (redTarget) {
								redirect = dup2(targetFD, 1);
							}
							if (redSource) {
								redirect2 = dup2(sourceFD, 0);
							}

							//Execute command
							if (execvp(*argument, argument) == -1) {
								printf("%s: no such file or directory\n", *argument);
								fflush(stdout);
								exit(1);
							}

							//Close file streams after redirection
							if (redTarget) {
								close(targetFD);
							}
							if (redSource) {
								close(sourceFD);
							}
							break;
						}

						//Parent
						default: {
							waitpid(-1, &childExitMethod, WNOHANG);
							printf(": ");
							fflush(stdout);
							//Trigger SIGCHILD
							sigaction(SIGCHLD, &SIGCHLD_action, NULL);
							break;
						}
					}
				}

				//Foreground process
				else {

					//Disable SIGCHILD
					sigaction(SIGCHLD, &ignore_action, NULL);

					//Ignore '&' in foreground only mode
					if ((foregroundOnly) && (argument[j - 1] == '&')) {
						argument[j - 1] = NULL;
					}

					//Fork
					spawnPID = fork();

					//Follow child process
					switch (spawnPID) {

						//Error
						case -1: { 
							perror("ERROR IN FORKING PROCESS\n");
							fflush(stderr);
							exit(1);
							break;
						}

						//Success
						case 0: {
							//Redirection
							if (redTarget) {
								redirect = dup2(targetFD, 1);
							}
							if (redSource) {
								redirect2 = dup2(sourceFD, 0);
							}

							//Execute command
							if (execvp(*argument, argument) == -1) {
								printf("%s: no such file or directory\n", *argument);
								fflush(stdout);
								exit(1);
							}

							//Close file streams after redirection
							if (redTarget) {
								close(targetFD);
							}
							if (redSource) {
								close(sourceFD);
							}
							break;
						}

						//Parent
						default: {
							waitpid(spawnPID, &childExitMethod, 0);
							printf(": ");
							fflush(stdout);
							break;
						}
					}
				}
			}
		}
	}

	//Prevent memory leaks
	free(command);
	command = NULL;
	return 0;
}
