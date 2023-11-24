// Supports I/O redirection, but cannot append to files (couldn't complete)

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LINESIZE 1024

int in = 0;
int out = 0;
int new_pos;
int iostate;
char* temp_out = NULL;
char* temp_in = NULL;

// Read what the user types into your "shell"
static char* read_line(void){
    int pos = 0;
    int maxSize = LINESIZE;
    char* input = malloc(sizeof(char) * maxSize); // whatever the user types
    in = 0;
    out = 0;
    char c;

    if(!input){
        fprintf(stderr, "unable to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "$ ");

    while (1){
        c = getchar();
        if (c != EOF && c != '\n'){
            if (iostate != 0){
                if (iostate == 1) { // get input redirection filename
                    if (c != ' '){
                        temp_in[new_pos] = c;
                        new_pos++;
                    }

                    if (strstr(temp_in, ".txt") != NULL){
                        iostate = 0;
                    }
                }
                else { // get output redirection filename
                    if (c != ' ') {
                        temp_out[new_pos] = c;
                        new_pos++;
                    }

                    if (strstr(temp_out, ".txt") != NULL) {
                        iostate = 0;
                    }
                }
            }
            else if (c == '<') { // input redirection
                iostate = 1;
                in = 1;
                new_pos = 0;
            }
            else if (c == '>') { // output redirection
                iostate = 2;
                out = 1;
                new_pos = 0;
            }
            else {
                input[pos] = c;
                pos++;
            }
        }
        else {
            input[pos] = '\0';
            return input;
        }

        // exceed allocated memory
        if (pos >= maxSize){
            maxSize += maxSize;
            input = realloc(input, maxSize);
            if (input == 0){    // realloc fails
                fprintf(stderr, "realloc failed\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

#define DELIM " \t\n"
// Handles the parsing of the input, assuming that 
// commands are separated by whitespace only
static char** split_command(char* line){
    int maxSize = LINESIZE;
    int pos = 0;
    char** input = malloc(sizeof(char*) * maxSize);
    char* token;

    if(!input){
        fprintf(stderr, "unable to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, DELIM);
    while (token != NULL){
        input[pos] = token;
        pos++;

        if (pos >= maxSize){
            maxSize += maxSize;
            input = realloc(input, maxSize);
            if (input == 0) {
                fprintf(stderr, "realloc failed\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIM);
    }
    return input;
}

// Runs the processes 
static int run_process(char* argv[]){
    pid_t child_pid, wpid;
    int status;

    child_pid = fork();
    if (child_pid == -1)
    {   /* fork() failed */
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0)
    {   /* This is the child */
        /* Child does some work and then terminates */

        if (in == 1) { // if '<' is detected in input
            int fd0 = open(temp_in, O_RDONLY);    // input.txt
            dup2(fd0, STDIN_FILENO);
            close(fd0);
        } 

        if (out == 1) { // if '>' is detected in input
            int fd1 = open(temp_out, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);    // write only
            dup2(fd1, STDOUT_FILENO);
            close(fd1);
        }

        int ret;
        ret = execvp(argv[0], argv);

        if(ret == -1){
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "Unknown error code %d from execl\n", ret);     // should never happen
        exit(EXIT_FAILURE);
    }
    else {   /* This is the parent */
        do {
            wpid = waitpid(child_pid, &status, WUNTRACED
            #ifdef WCONTINUED               /* Not all implementations support this */
                | WCONTINUED
            #endif
            );
            if (wpid == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status)) {
                fprintf(stderr, "child exited, status=%d\n", WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status)) {
                fprintf(stderr, "child killed (signal %d)\n", WTERMSIG(status));
            }
            else if (WIFSTOPPED(status)) {
                fprintf(stderr, "child stopped (signal %d)\n", WSTOPSIG(status));
                #ifdef WIFCONTINUED             /* Not all implementations support this */
            }
            else if (WIFCONTINUED(status)) {
                fprintf(stderr, "child continued\n");
            #endif
            } else {                   /* Non-standard case -- may never happen */
                fprintf(stderr, "Unexpected status (0x%x)\n", status);
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

// Runs the cd command
static int shell_cd(char* argv[]){
    if(chdir(argv[0]) != 0){
        perror("cd");
    }
    return 1;
}

// Runs the exit command
static int shell_exit(char* argv[]){
    return 0;
}

// Runs the shell
static void shell(void){
    char* cd = "cd";
    char* exit = "exit";
    char* line;
    char** args;
    int running;

    do {
        temp_in = malloc(sizeof(char) * LINESIZE);
        temp_out = malloc(sizeof(char) * LINESIZE);

        line = read_line();
        args = split_command(line);

        if (strcmp(args[0], cd) == 0){  // cd
            running = shell_cd(&args[1]);
        } else if (strcmp(args[0], exit) == 0){ // exit
            running = shell_exit(&args[0]);
        } else {
            running = run_process(args);
        }

        free(line);
        free(args);
    } while (running);    
}

int main(int argc, char* argv[]){
    printf("This is a terminal, enter your command: \n");
    shell();

    return EXIT_SUCCESS;
}