#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//All Helper Function Definitions
void printMEM();
int ParseOp1(char *IR);
int ParseOp2(char *IR);
int ParseOP1andOP2Imm(char *IR);
int ParseOp1Reg(char *IR);
int ParseOp2Reg(char *IR);
int FetchData(int Memory_Location);
void WriteData(int data, int loc);

//Project 2 Additional Functions used for scheduling
struct PCB *GetNextProcess(struct PCB **RQ);
void DeletePCB(struct PCB *Current);
void MvToTail(struct PCB *Current, struct PCB **RQT);
void PrintQ(struct PCB *Head);
int ExecuteProc(struct PCB *CurrentProc);
void RestoreState(struct PCB *NextProc);
void SaveState(struct PCB **PrevProc);
void LoadProgram(int PID, struct PCB **tmp);

//All OPCode definitions
void OP0(char *IR);     //Load Pointer Immediate
void OP1(char *IR);     //Add to Pointer Immediate
void OP2(char *IR);     //Subtract from Pointer Immediate
void OP3(char *IR);     //Load Accumulator Immediate
void OP4(char *IR);     //Load Accumulator Register Addressing
void OP5(char *IR);     //Load Accumulator Direct Addressing
void OP6(char *IR);     //Store Accumulator Register Addressing
void OP7(char *IR);     //Store Accumulator Direct Addressing
void OP8(char *IR);     //Store Register to memory: Register Addressing
void OP9(char *IR);     //Store Register to Memory: Direct Addressing
void OP10(char *IR);    //Load Register from memory: Register Addressing
void OP11(char *IR);    //Load Register from memory: Direct Addressing
void OP12(char *IR);    //Load Register R0 Immediate
void OP13(char *IR);    //Register to Register Transfer
void OP14(char *IR);    //Load Accumulator from Register
void OP15(char *IR);    //Load Register from Accumulator
void OP16(char *IR);    //Add Accumulator Immediate
void OP17(char *IR);    //Subtract Accumulator Immediate
void OP18(char *IR);    //Add contents of Register to Accumulator
void OP19(char *IR);    //Subtract contents of Register from Accumulator
void OP20(char *IR);    //Add Accumulator Register Addressing
void OP21(char *IR);    //Add Accumulator Direct Addressing
void OP22(char *IR);    //Subtract from Accumulator Register Addressing
void OP23(char *IR);            //Subtract from Accumulator Direct Addressing
void OP24(char *IR, char *PSW); //Compare Equal Register Addressing
void OP25(char *IR, char *PSW); //Compare Less Register Addressing
void OP26(char *IR, char *PSW); //Compare Greater Register Addressing
void OP27(char *IR, char *PSW); //Compare Greater Immediate
void OP28(char *IR, char *PSW); //Compare Equal Immediate
void OP29(char *IR, char *PSW); //Compare Less Immediate
void OP30(char *IR, char *PSW); //Compare Register Equal
void OP31(char *IR, char *PSW); //Compare Register Less
void OP32(char *IR, char *PSW); //Compare Register Greater
void OP33(char *IR, char *PSW, short int *PC); //Branch Conditional True
void OP34(char *IR, char *PSW, short int *PC); //Branch Conditional False
void OP35(char *IR, char *PSW, short int *PC); //Branch Unconditional
void OP99(); //Halt


/* All of the OP functions are stored in pointer arrays, this way I have few checks that possibly doing 36 when I want to find the HALT statement (OP99) using the switch statement.
The OP codes are called on by their position in the array. OP0 is in arr[0], OP1 is in arr[1] ...
The functions calls that go through the array will pass through as many arguments that can be taken by the original function, OP0 will take the first parameter (char *IR), then ignore (char *PSW)
And for function OP24 it will take arguments available until it is full by order, its first will be the IR, then it will take the second (PSW). ::Basically this means the setup work as is
I check (where the old switch statement would go) if the OP belongs to p1, p2, or p3; since their arguments are not compatible with the pattern (IR, PSW)
If all OP codes used all parameters*/

//Pointers to all functions (not OP99) NOTE: OP35 does not use PSW, I put it in the function to make this 1 array, otherwise I would need 2 arrays to compensate its different parameter structure
void (*opPointers[]) (char *IR, char *PSW, short int *PC) = {OP0, OP1, OP2, OP3, OP4, OP5, OP6, OP7, OP8, OP9, OP10, OP11, OP12, OP13, OP14, OP15, OP16, OP17, OP18, OP19, OP20,
                                                                OP21, OP22, OP23, OP24, OP25, OP26, OP27, OP28, OP29, OP30, OP31, OP32, OP33, OP34, OP35};

//Array of all the file names, or program names to load. If you want to load different programs, change these charArrays
const char *fileNames[10] = {"fib_seq1", "fib_seq2", "fib_seq3", "fib_seq4", "fib_seq5", "fib_seq6",
                            "fib_seq7", "fib_seq8", "fib_seq9", "fib_seq10"};

//PCB Struct for process storing
struct PCB {
    struct PCB *Next_PCB;
    int PID, IC;
    short int PC;
    short int P[4];
    int R[4];
    char PSW[2];
    int ACC;
    int BaseReg, LimitReg;
};

/*These are variables representing the VM itself*/
char IR[6];
short int PC = 0;

/*Pointer arrays don't seem to work, using original Implementation with arrays*/
short int P[4];
int R[4];

int BaseRegister, LimitRegister;

int ACC;
char PSW[2];
char memory [1000][6];        //this is the program memory for first program
short int opcode;            //nice to know what we are doing

/*These variables are associated with the implementation of the VM*/
int i, j, k;
int ProgSize;
char input_line [7];

main(int argc, char *argv[]){
 //Variables used by OS
    struct PCB *RQ, *tmp, *RQT, *Current;

    RQ = (struct PCB *) malloc (sizeof (struct PCB));
    RQ->PID = 0;
    RQ->IC = 2;
    tmp = RQ;
    for(i = 1; i < 10; i++){
        tmp->Next_PCB = (struct PCB *) malloc (sizeof (struct PCB));
        tmp->Next_PCB->PID = i;
        tmp->Next_PCB->IC = (rand() % 10) + 1; //rand returns 0 .. MAX
        tmp->Next_PCB->Next_PCB = NULL;
        tmp = tmp->Next_PCB;
    }

    RQT = tmp;
    RQT->Next_PCB = NULL;  //make it null x2

	tmp = RQ;
	for (i = 0; i < 10 ; i++){
		 LoadProgram( i, &tmp);
		 printf("LimitReg = %d. IC = %d\n\n",tmp->LimitReg, tmp->IC);
		 //tmp->IC = i + 2 ;
		 tmp = tmp->Next_PCB;
    }
	printf("Now let's EXECUTE IT! WOOOEEEEEE!!!\n");

	while(1){
        Current = GetNextProcess(&RQ);
		RestoreState(Current);
        printf("\nPROGRAM %s with PID [%d] HAS BEEN RESTORED, STARTING EXECUTION WITH IC = [%d]\n", fileNames[Current->PID], Current->PID, Current->IC);

		int Completed = ExecuteProc(Current);
		printf("PROGRAM %s with PID [%d] RETURNING CONTROL TO THE OS.\n", fileNames[Current->PID], Current->PID);

        if (Completed){
            printf("Removing PID %d\n", Current->PID);
            DeletePCB(Current);
        }else{
            SaveState(&Current);
            printf("Moving PID %d to TAIL\n", Current->PID);
            MvToTail(Current, &RQT);
            printf("RQT is %d\n", RQT->PID);
            if(RQ == NULL){
                RQ = RQT;
            }
        }
        PrintQ(RQ) ;
        //sleep(1) ;
        if (RQ == NULL){
            printf("All programs finished execution. OS is going to sleep...\nGoodNight!\n");
            break;
        }
    }
}

/*--------------ALL PROJECT2 SCHEDULING FUNCTIONS--------------*/

struct PCB *GetNextProcess(struct PCB **RQ){//Gets the process at the head of the RW
                                            //Disconnects it from the RQ and returns it
    struct PCB *temp;                       //Temp pointer to PCB
    temp = (*RQ);                           //sets temp to RQ Head
    (*RQ) = (*RQ)->Next_PCB;            //Moves head along RQ
    temp->Next_PCB = NULL;                  //completely disconnects PCB
    return temp;                            //returns the PCB
}

void DeletePCB(struct PCB *Current){        //Deletes a fully disconnected PCB
    free(Current);
}

void MvToTail(struct PCB *Current, struct PCB **RQT){   //Places disconnected PCB to tail
    (*RQT)->Next_PCB = Current;             //Sets tail's next PCB to current
    (*RQT) = (*RQT)->Next_PCB;              //Moves tail to last position
    (*RQT)->Next_PCB = NULL;     //TEST: for weird infinite RQ
}

void PrintQ(struct PCB *Head){              //Prints the RQ
    struct PCB *temp;                       //Temp PCB pointer
    temp = Head;                            //Sets temp to head
    if(temp != NULL){                       //If it is not null
        printf("[%d]-->", temp->PID, temp->IC);
    }else{                                  //Print (like in HW2)
        return;
    }
    while(temp->Next_PCB != NULL){
        printf("[%d]-->", temp->Next_PCB->PID, temp->Next_PCB->IC);
        temp = temp->Next_PCB;
    }
    printf("\n");
}

int ExecuteProc(struct PCB *CurrentProc){

	int tempIC = CurrentProc->IC;           //Get a copy of IC
	while (1){                          //Execute forever, until interrupted
        for (i = 0; i < 6 ; i++){
			IR[i] = memory[PC][i] ;
        }
        opcode = ((int) (IR[0])- 48) * 10 ;
        opcode += ((int) (IR[1])- 48) ;
        printf("\n Process PID = [%d] is executing!: IC = %d, EAR = %d, OPCODE = %d\n\n", CurrentProc->PID, tempIC, PC, opcode) ;

		/* You need to put in the case statements for the remaining opcodes */
		if(opcode == 99){                       //If it is halt, then halt
            OP99();
            return 1;                           //Return 1 to say its complete

		}else if(tempIC <= 0){                  //If no more allowed
		    return 0;                           //return not done

        }else{
            (*opPointers[opcode])(&IR, &PSW, &PC);//Otherwise do whatever the OPCode program is
            PC++;                                 //Add to PC after opcode completion
            tempIC--;                             //Subtract from tempIC
            printf("-------------------------------------------------------"); //Formatting to improve readability
		}
    }
}
void RestoreState(struct PCB *NextProc){
    PC = NextProc->PC + NextProc->BaseReg;  //Restore PC as Effective Address Register
    int c;
    for(c = 0; c<4; c++){                   //I used arrays for PREG and REG
        P[c] = NextProc->P[c];              //Restore all 4 P[]
        R[c] = NextProc->R[c];              //Restore all 4 R[]
    }
    PSW[0] = NextProc->PSW[0];              //Copy PSW[] individually
    PSW[1] = NextProc->PSW[1];
    ACC = NextProc->ACC;                    //Restore ACC
    BaseRegister = NextProc->BaseReg;       //Restore BaseReg
    LimitRegister = NextProc->LimitReg;     //Restore LimitReg
}

void SaveState(struct PCB **PrevProc){
    (*PrevProc)->PC = PC - (*PrevProc)->BaseReg; //Copy EAR as normal PC
    int c;
    for(c = 0; c<4; c++){                   //Copy R[] and P[]
        (*PrevProc)->P[c] = P[c];
        (*PrevProc)->R[c] = R[c];
    }
    (*PrevProc)->PSW[0] = PSW[0];
    (*PrevProc)->PSW[1] = PSW[1];
    (*PrevProc)->ACC = ACC;
    //(*PrevProc)->BaseReg = BaseRegister;      //No Reason to copy these?
    //(*PrevProc)->LimitReg = LimitRegister;    //They are set when loaded
}

void LoadProgram(int PID, struct PCB **tmp){
    int i, fp ;
	int program_line = 100 * PID;
	(*tmp)->BaseReg  = program_line;
	(*tmp)->LimitReg = program_line + 100;
    fp = open(fileNames[PID], O_RDONLY) ; //always check the return value.
    printf("Open is %d\n", fp) ;

        if (fp < 0){ //error in read
            printf("Could not open file\n");
            exit(0) ;
        }
 	int ret = read (fp, input_line, 7 ) ; //returns number of characters read`

	while (1){
        if (ret <= 0){ //indicates end of file or error
            break ; //breaks out of infinite loop
        }
        printf("Copying %s's program line %d into memory\n", fileNames[PID], program_line);
        for (i = 0; i < 6 ; i++){
            memory[program_line][i] = input_line[i] ;
            printf("%c ", memory[program_line][i]) ;
        }
        printf("\n") ;

        ret = read (fp, input_line, 7 ) ;

        //if the firat character is a 'Z' then you are reading data.
        //No more program code so break out of loop

        if(input_line[0] == 'Z'){
            break ;
        }
        program_line++ ; //now at a new line in the prog
    }

  printf("Read in Code. Closing File\n") ;
  close(fp) ;
}


/*--------------ALL HELPER FUNCTIONS BELOW--------------*/

//This function returns the integer value of operand 1
//when this operand is an immediate two-byte integer.

int ParseOp1 (char *IR){
    return ((int)(IR[2]-48))*10 + (int)(IR[3]-48);
}

// returns the integer value of operand 2 when this operand is a two-byte integer.
int ParseOp2 (char *IR){
    return ((int)(IR[4]-48))*10 + (int)(IR[5]-48);
}

//returns the integer value of operands 1 and 2 combined to form a 4-byte integer.
int ParseOP1andOP2Imm(char *IR){
    return ((int)(IR[2]-48))*1000 + ((int)(IR[3]-48))*100 +
    ((int)(IR[4]-48))*10 + (int)(IR[5]-48);
}

// returns the register number of the register used as operand  1 of an instruction.
// Can be either Pointer or General-Purpose register.
int ParseOp1Reg (char *IR){
    return (int)(IR[3]-48);
}

// returns the register number of a register used as operand  2 of an instruction.
// Can be either a Pointer or General-Purpose register.
int ParseOp2Reg (char *IR){
    return (int)(IR[5]-48);
}

// returns the data stored at memory location Memory_Location
int FetchData(int Memory_Location){
    int i;
    char mem[6];                            //Temp mem holder
    for(i = 0; i<6; i++){
        mem[i] = memory[Memory_Location][i];//Write actual mem into temp mem
    }
    return ParseOP1andOP2Imm(mem);          //Parse the last 4 digits of memory
}

//Writes the int data to the in location in memory (converts to char)
void WriteData(int data, int loc){
    int i;
    char mem[6];                            //Temp memory holder
    mem[0] = 'Z';                           //Memory Locations start with ZZ
    mem[1] = 'Z';
    mem[2] = (data/1000) + '0';             //Parse out only the single digit we use
    mem[3] = ((data%1000)/100) + '0';       //by dividing and modulo
    mem[4] = (((data%1000)%100)/10) + '0';  //then add + '0' to easy convert the int val to a char
    mem[5] = (((data%1000)%100)%10) + '0';

    for(i = 0; i<6; i++){                   //Write the temp mem[] into actual memory
        memory[loc][i] = mem[i];
    }
}

//Prints out the contents of the IR on the same line.
void PrintIR(char *IR){
    int i;
    for(i = 0; i<6; i++){
        printf("%c", IR[i]);
    }
    printf("\n");
}

//Prints out the contents of memory from row 0 to the end of used memory + 1
//This should print out all instructions and data stored in memory.
void printMEM(){
    int x, i, usedMem;
    x = 0;
    while (usedMem){                    //Conditional to check if mem was used
        usedMem = 0;                    //Always assume there is no more until proven wrongly
        printf("%d:  ", x);             //Print line number
        for(i=0; i<6; i++){
            printf("%c", memory[x][i]); //Print memory character
            if(memory[x][i] != 0){      //If there is ever something other than 0 in the memory then it was used
                usedMem = 1;            //Based on the idea that GCC fills the uninitialized array with "0"'s
            }
        }
        printf("\n");
        x++;                            //Increment line number
    }
}

/*--------------ABOUT THE OP CODES--------------
All OP codes use a generic fashion represented by the first one given to us.
PREG = Pointer Register number that is used in this OP
REG = Register number that is used in this OP
VAL = the "XX" or "XXXX" int value input in the IR after being parsed
LOC = Memory location, and is used in Direct Addressing
Very few OP codes need to implement other variables. If they do comments in that OP will explain*/


/*--------------ALL OP CODE FUNCTION BELOW--------------*/

OP0(char *IR){          //Load Pointer Immediate
    int PREG, VAL ;
    printf("Opcode = 00. Load Pointer Immediate\n") ;
    PrintIR(IR) ;

    PREG = ParseOp1Reg(IR) ;
    VAL = 	ParseOp2 (IR) ;

    P[PREG] = VAL;

    printf("Loaded P[%d] with immediate value %d\n", PREG, VAL ) ;
}
void OP1(char *IR){     //Add to Pointer Immediate
    int PREG, VAL;
    printf("Opcode = 01. Add to Pointer Immediate\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = ParseOp2(IR);

    P[PREG] += VAL;

    printf("Added immediate value %d to P[%d] (=%d).\n", VAL, PREG, P[PREG]);
}
void OP2(char *IR){     //Subtract from Pointer Immediate
    int PREG, VAL;
    printf("Opcode = 02. Subtract From Pointer Immediate\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = ParseOp2(IR);

    P[PREG] -= VAL;

    printf("Subtracted immediate value %d from P[%d] (= %d).\n", VAL, PREG, P[PREG]);
}
void OP3(char *IR){     //Load Accumulator Immediate
    int VAL;
    printf("Opcode = 03. Load Accumulator: Immediate\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);

    ACC = VAL;

    printf("Loaded ACC with immediate value %d.\n", VAL);
}
void OP4(char *IR){     //Load Accumulator Register Addressing
    int PREG, VAL;
    printf("Opcode = 04. Load Accumulator: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = FetchData(P[PREG] + BaseRegister);

    ACC = VAL;
    printf("Loaded ACC with contents of memory at P[%d]. (ACC = %d).\n", PREG, VAL);
}
void OP5(char *IR){     //Load Accumulator Direct Addressing
    int LOC, VAL;
    printf("Opcode = 05. Load Accumulator Direct Addressing.\n");
    PrintIR(IR);

    LOC = ParseOp1(IR);
    VAL = FetchData(LOC + BaseRegister);

    ACC = VAL;
    printf("Loaded ACC with contents of memory at M[%d]. (ACC == %d).\n", LOC, VAL);
}
void OP6(char *IR){     //Store Accumulator Register Addressing
    int PREG;
    printf("Opcode = 06. Store Accumulator: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    WriteData(ACC, P[PREG] + BaseRegister);

    printf("Stored contents of ACC (%d), to memory location of P[%d] (= %d).\n", ACC, PREG, P[PREG]);
}
void OP7(char *IR){     //Store Accumulator Direct Addressing
    int LOC;
    printf("Opcode = 07. Store Accumulator: Direct Addressing.\n");
    PrintIR(IR);

    LOC = ParseOp1(IR);
    WriteData(ACC, LOC + BaseRegister);

    printf("Stored contents of ACC (%d), to memory location [%d].\n", ACC, LOC);
}
void OP8(char *IR){     //Store Register to memory: Register Addressing
    int PREG, REG;
    printf("Opcode = 08. Store Register to Memory: Register Addressing.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    PREG = ParseOp2Reg(IR);
    WriteData(R[REG], P[PREG] + BaseRegister);

    printf("Stored contents of R[%d] (= %d), to memory location of P[%d] (= %d).\n", REG, R[REG], PREG, P[PREG]);
}
void OP9(char *IR){     //Store Register to Memory: Direct Addressing
    int REG, VAL;
    printf("Opcode = 09. Store Register to Memory: Direct Addressing.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    VAL = ParseOp2(IR);
    WriteData(R[REG], VAL + BaseRegister);

    printf("Stored contents of R[%d] (= %d), to memory location [%d].\n", REG, R[REG], VAL);
}
void OP10(char *IR){    //Load Register from memory: Register Addressing
    int PREG, REG;
    printf("Opcode = 10. Load Register from Memory: Register Addressing.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    PREG = ParseOp2Reg(IR);
    R[REG] = FetchData(P[PREG] + BaseRegister);

    printf("Loaded Memory at P[%d] (= %d), into R[%d].\n", PREG, P[PREG], REG);
}
void OP11(char *IR){    //Load Register from memory: Direct Addressing
    int LOC, REG;
    printf("Opcode = 11. Load Register from Memory: Direct Addressing.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    LOC = ParseOp2(IR);
    R[REG] = FetchData(LOC + BaseRegister);

    printf("Loaded Memory at [%d] (= %d) into R[%d].\n", LOC, R[REG], REG);
}
void OP12(char *IR){    //Load Register R0 Immediate
    int VAL;
    printf("Opcode = 12. Load Register 0 Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    R[0] = VAL;

    printf("Loaded value of [%d] into R[0].\n", VAL);
}
void OP13(char *IR){    //Register to Register Transfer
    int REG, RecREG;
    printf("Opcode = 13. Register to Register Transfer.\n");
    PrintIR(IR);

    RecREG = ParseOp1Reg(IR);       //RecREG is the Receiving Register for the Transfer
    REG = ParseOp2Reg(IR);
    R[RecREG] = R[REG];

    printf("Assigned R[%d] the value of R[%d]. (%d)\n", RecREG, REG, R[REG]);
}
void OP14(char *IR){    //Load Accumulator from Register
    int REG;
    printf("Opcode = 14. Load Accumulator from Register.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    ACC = R[REG];

    printf("Accumulator assigned the contents of R[%d]. (%d)\n", REG, R[REG]);
}
void OP15(char *IR){    //Load Register from Accumulator
    int REG;
    printf("Opcode = 15. Load Register from Accumulator.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    R[REG] = ACC;

    printf("R[%d] assigned the contents of the Accumulator. (%d)\n", REG, ACC);
}
void OP16(char *IR){    //Add Accumulator Immediate
    int VAL;
    printf("Opcode = 16. Add Accumulator Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    ACC += VAL;

    printf("Added value [%d], to Accumulator.\n", VAL);
}
void OP17(char *IR){    //Subtract Accumulator Immediate
    int VAL;
    printf("Opcode = 17. Subtract Accumulator Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    ACC -= VAL;

    printf("Subtracted value [%d], from Accumulator.\n", VAL);
}
void OP18(char *IR){    //Add contents of Register to Accumulator
    int REG;
    printf("Opcode = 18. Add contents of Register to Accumulator.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    ACC += R[REG];

    printf("Added contents of R[%d] (= %d), to Accumulator.\n", REG, R[REG]);
}
void OP19(char *IR){    //Subtract contents of Register from Accumulator
    int REG;
    printf("Opcode = 19. Subtract contents of Register from Accumulator.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    ACC -= R[REG];

    printf("Subtracted contents of R[%d] (= %d), from Accumulator.\n", REG, R[REG]);
}
void OP20(char *IR){    //Add Accumulator Register Addressing
    int PREG;
    printf("Opcode = 20. Add Accumulator: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    ACC += FetchData(P[PREG] + BaseRegister);

    printf("Added contents of memory at P[%d] (= %d), to Accumulator.\n", PREG, FetchData(P[PREG]));
}
void OP21(char *IR){    //Add Accumulator Direct Addressing
    int VAL;
    printf("Opcode = 21. Add Accumulator: Direct Addressing.\n");
    PrintIR(IR);

    VAL = ParseOp1(IR);
    ACC += FetchData(VAL + BaseRegister);

    printf("Added contents of memory at [%d] (= %d), to Accumulator.\n", VAL, FetchData(VAL));
}
void OP22(char *IR){    //Subtract from Accumulator Register Addressing
    int PREG;
    printf("Opcode = 22. Subtract from Accumulator: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    ACC -= FetchData(P[PREG] + BaseRegister);

    printf("Subtracted contents of memory at P[%d] (= %d), from Accumulator.\n", PREG, FetchData(P[PREG]));
}
void OP23(char *IR){    //Subtract from Accumulator Direct Addressing
    int VAL;
    printf("Opcode = 23. Subtract from Accumulator: Direct Addressing.\n");
    PrintIR(IR);

    VAL = ParseOp1(IR);
    ACC -= FetchData(VAL + BaseRegister);

    printf("Subtracted contents of memory at [%d] (= %d), from Accumulator.\n", VAL, FetchData(VAL));
}
void OP24(char *IR, char *PSW){ //Compare Equal Register Addressing
    int PREG, VAL;
    printf("Opcode = 24. Compare Equal: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = FetchData(P[PREG] + BaseRegister);
    if(ACC == VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '=', ACC to memory location store in P[%d]. %d == %d.\n", PREG, ACC, VAL);
}
void OP25(char *IR, char *PSW){ //Compare Less Register Addressing
    int PREG, VAL;
    printf("Opcode = 25. Compare Less: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = FetchData(P[PREG] + BaseRegister);
    if(ACC < VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '<', ACC to memory location store in P[%d]. %d < %d.\n", PREG, ACC, VAL);
}
void OP26(char *IR, char *PSW){ //Compare Greater Register Addressing
    int PREG, VAL;
    printf("Opcode = 26. Compare Greater: Register Addressing.\n");
    PrintIR(IR);

    PREG = ParseOp1Reg(IR);
    VAL = FetchData(P[PREG] + BaseRegister);
    if(ACC > VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '>' ACC to memory location store in P[%d]. %d > %d.\n", PREG, ACC, VAL);
}
void OP27(char *IR, char *PSW){ //Compare Greater Immediate
    int VAL;
    printf("Opcode = 27. Compare Greater: Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    if(ACC > VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '>', ACC to Immediate Value. %d > %d.\n", ACC, VAL);
}
void OP28(char *IR, char *PSW){ //Compare Equal Immediate
    int VAL;
    printf("Opcode = 28. Compare Equal: Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    if(ACC == VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '=', ACC to Immediate Value. %d == %d.\n", ACC, VAL);
}
void OP29(char *IR, char *PSW){ //Compare Less Immediate
    int VAL;
    printf("Opcode = 29. Compare Less: Immediate.\n");
    PrintIR(IR);

    VAL = ParseOP1andOP2Imm(IR);
    if(ACC < VAL){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '<', ACC to Immediate Value. %d < %d.\n", ACC, VAL);
}
void OP30(char *IR, char *PSW){ //Compare Register Equal
    int REG;
    printf("Opcode = 30. Compare Register Equal.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    if(ACC == REG){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '=', ACC to R[%d]. %d == %d.\n", REG, ACC, R[REG]);
}
void OP31(char *IR, char *PSW){ //Compare Register Less
    int REG;
    printf("Opcode = 31. Compare Register Less.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    if(ACC < REG){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '<', ACC to R[%d]. %d == %d.\n", REG, ACC, R[REG]);
}
void OP32(char *IR, char *PSW){ //Compare Register Greater
    int REG;
    printf("Opcode = 32. Compare Register Greater.\n");
    PrintIR(IR);

    REG = ParseOp1Reg(IR);
    if(ACC > REG){
        PSW[0] = 1;
    }else{
        PSW[0] = 0;
    }
    printf("Compared '>', ACC to R[%d]. %d == %d.\n", REG, ACC, R[REG]);
}
void OP33(char *IR, char *PSW, short int *PC){ //Branch Conditional True
    int VAL;
    printf("Opcode = 33. Branch Conditional True.\n");
    PrintIR(IR);

    VAL = ParseOp1(IR);
    if(PSW[0] == 1){
        *PC = (short int)VAL -1;    //-1 because PC is incremented after OP Call
    }
    printf("PSW = [%d]. Branch Conditional Jump: %s. New PC = %d\n", PSW[0], (PSW[0] ? "Success" : "Failed"), *PC + 1);
}
void OP34(char *IR, char *PSW, short int *PC){ //Branch Conditional False
    int VAL;
    printf("Opcode = 34. Branch Conditional False.\n");
    PrintIR(IR);

    VAL = ParseOp1(IR);
    if(PSW[0] == 0){
        *PC = (short int)VAL -1;    //-1 because PC is incremented after OP Call
    }
    printf("PSW = [%d]. Branch Conditional Jump: %s. New PC = %d\n", PSW[0], (PSW[0] ? "Failed" : "Success"), *PC + 1);
}
void OP35(char *IR, char *PSW, short int *PC){ //Branch Unconditional
    int VAL;
    printf("Opcode = 34. Branch Unconditional.\n");
    PrintIR(IR);

    VAL = ParseOp1(IR);
    *PC = (short int)VAL -1;    //-1 because PC is incremented after OP Call

    printf("Branch Unconditional. New PC = %d\n", *PC + 1);
}
void OP99(){                     //Halt
    printf("OP Code is 99: Dumping Memory and Exit\n");
    printMEM();
}
