//
// Created by 钱苏澄 on 2018/7/3.
//

#ifndef MYMIPSRE_PARSER_H
#define MYMIPSRE_PARSER_H

#include <cstring>
#include <vector>
#include <map>

#include "Utility.h"
#include "InsMem.h"
#include "Global.h"

// #define RUN2

using namespace std;


/**
 * A class that is used to scan a line of code and interpret it into a vector of tokens.
 * It returns true if a line isn't trivial, false otherwise.
 * It is able to recognize comment, tab, comma...
 * */
class Scanner {
private:
		string *line;
		int index, len;
public:
		Scanner() {
				index = len = 0;
				line = nullptr;
		}

		/**
		 * Specify a new line to process.
		 * */
		void setLine(string &src_line) {
				line = &src_line;
				len = line->length();
				index = 0;
		}

		/**
		 * process current line to end, and ends up with a tokens vector.
		 * If this line is trivial, return false, otherwise return true.
		 * */
		bool getTokens(vector<string> &tokens) {
				string token;
				while ((token = getNextToken()) != "") {
						tokens.push_back(token);
				}
				if (tokens.size() == 0) return false;
				else return true;
		}

private:
		/**
		 * Get next token which is separated by " "、 ","、and tab.
		 * If the token returned is "", line-processing is finished.
		 * */
		string getNextToken() {
				string str = "";

				/// 跳过前面没有意义的。
				while (index < len && ((*line)[index] == ' ' || (*line)[index] == '\t' || (*line)[index] == ',')) {
						++index;
				}
				/**
				 * 注意，我们在爬字符串的时候要非常小心，否则就是完蛋。
				 * 直接特判。
				 * */

				if (index < len && (*line)[index] != '\"') {
						while (index < len && (*line)[index] != ' ' && (*line)[index] != '\t' && (*line)[index] != ',') {
								str += (*line)[index];
								++index;
						}
				} else if (index < len && (*line)[index] == '\"') {
						while (index < len) {
								str += (*line)[index];
								++index;
						}
				}
				return str;
		}
};

/**
 * This is a class that parse the text source code and fill instructionPool.
 * It also allocates static_variable.
 * */
class Parser {
    enum statusType {
        DATA, TEXT
    };
private:
    Scanner scanner;
    statusType status;

private:

    /**
     * This tokens of instruction is for static_data_init, simulate in memory.
     * */
    void static_data_init(vector<string> &tokens) {
        /// align memory.hp pointer.
        if(tokens[0] == ".align") {
            int shift_exp = str2int(tokens[1]);
            int dist = (1 << shift_exp);
            int memPos = 0;
            while(memPos < mainMemory->hp) memPos += dist;
            for(int i = mainMemory->hp; i < memPos; ++i) {
                mainMemory->storage[i] = 0;
            }
            mainMemory->hp = memPos;
        }
        /// store string without \0.
        else if(tokens[0] == ".ascii") {
            /// 去除双引号。
            string static_str = getString(tokens[1]);
            for(int i = 0; i < static_str.length(); ++i) {
                mainMemory->storage[mainMemory->hp++] = static_str[i];
            }
        }
        /// store string with \0.
        else if(tokens[0] == ".asciiz") {
            string static_str = getString(tokens[1]);
            for(int i = 0; i < static_str.length(); ++i) {
                mainMemory->storage[mainMemory->hp++] = static_str[i];
            }
            // mainMemory->display();
            mainMemory->storage[mainMemory->hp++] = '\0';
        }
            /// store the following byte in memory.
        else if(tokens[0] == ".byte") {
            char byte_val;
            for(int i = 1; i < tokens.size(); ++i) {
                byte_val = (char) str2int(tokens[i]);
                mainMemory->storage[mainMemory->hp++] = byte_val;
            }
        }
            /// store the following half word in memory.
        else if(tokens[0] == ".half") {
            short half_val;
            for(int i = 1; i < tokens.size(); ++i) {
                half_val = (short) str2int(tokens[i]);
                memcpy(mainMemory->storage + mainMemory->hp, &half_val, sizeof(short));
                mainMemory->hp += sizeof(short);
            }
        }
            /// store the following word in memory.
        else if(tokens[0] == ".word") {
            int word_val;
            for(int i = 1; i < tokens.size(); ++i) {
                word_val = str2int(tokens[i]);
                memcpy(mainMemory->storage + mainMemory->hp, &word_val, sizeof(int));
                mainMemory->hp += sizeof(int);
            }
        }
            /// alloc n byte in mainMemory.
        else if(tokens[0] == ".space") {
            int alloc_space = str2int(tokens[1]);
            memset(mainMemory->storage + mainMemory->hp, 0, alloc_space * sizeof(char));
            mainMemory->hp += alloc_space;
        }
    }


public:
    Parser() {
        status = TEXT;
    }

		/**
		 * This function processes the whole source code read from cin.
		 * It creates instructions for TEXT, allocates data for DATA.
		 * Records positions for labels: TEXT label with vector index, DATA label with heap pointer offset.
		 * And finally replace labels with their index in instructionPool.
		 *
		 * And finally return the line index of main function.
		 * */
		int parse() {
				string src_line;
				vector<string> tokens;

				/// get tokens first.
				while (true) {
#ifdef RUN
						if (!getline(fp, src_line)) break;
						if (fp.eof()) break;
#else
						if(!getline(cin, src_line)) break;
#endif

						if (src_line[0] == '@')
								break;
						tokens.clear();
						scanner.setLine(src_line);

						if (scanner.getTokens(tokens)) {
								/// distinguish among TEXT, DATA, and LABEL.

								/// if it's a status change.
								if (tokens[0] == ".data") {
										status = DATA;
								} else if (tokens[0] == ".text") {
										status = TEXT;
								}
										/// if it creates a label.
								else if (tokens[0].back() == ':') {
										if (status == TEXT) {
												tokens[0].pop_back();
												/**
												 * here may be bugs, if src code ends with a label, then bomb!
												 * */
												lab2src.insert(pair<string, int>(tokens[0], instructionPool->size()));
										} else if (status == DATA) {
												tokens[0].pop_back();
												var2mem.insert(pair<string, int>(tokens[0], mainMemory->hp));
										}
								}
										/// if it does a normal instruction, distinguish it between ALU instruction and static_data creation.
								else {
										if (status == DATA) {
												static_data_init(tokens);     /// initialize static_data.
										} else if (status == TEXT) {
												Instruction newInstruction(opTable[tokens[0]], tokens);
												instructionPool->push_back(newInstruction);
										}
								}
						}
				}
				return lab2src["main"];
		}

		/**
		 * Parse the code to be the god damn shit.
		 * */
    void code_analyzer() {
				for(int i = 0; i < instructionPool->size(); ++i) {
						Instruction &ins = instructionPool->at(i);
						switch (ins.op) {
								case ADD: case ADDU: case ADDIU: case SUB: case SUBU:
								case XOR: case XORU: case REM: case REMU:
								case SEQ: case SGE: case SGT: case SLE: case SLT: case SNE:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										ins.ins_code[1] = regTable[ins.tokens[2]];
										ins.not_imm = ins.tokens[3][0] == '$';
										if(ins.not_imm)
												ins.ins_code[2] = regTable[ins.tokens[3]];
										else
												ins.ins_code[2] = str2int(ins.tokens[3]);
										break;
								case MUL: case MULU: case DIV: case DIVU:
										if(ins.tokens.size() == 4) {
												ins.not_imm = ins.tokens[3][0] == '$';
												ins.ins_code[0] = regTable[ins.tokens[1]];
												ins.ins_code[1] = regTable[ins.tokens[2]];
												if(ins.not_imm)
														ins.ins_code[2] = regTable[ins.tokens[3]];
												else
														ins.ins_code[2] = str2int(ins.tokens[3]);
										}
										else if(ins.tokens.size() == 3){
												ins.not_imm = ins.tokens[2][0] == '$';
												ins.ins_code[1] = regTable[ins.tokens[1]];
												if(ins.not_imm)
														ins.ins_code[2] = regTable[ins.tokens[2]];
												else
														ins.ins_code[2] = str2int(ins.tokens[2]);
										}
										else {
												cerr << "mul bug" << endl;
										}
										break;
								case NEG: case NEGU:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										ins.ins_code[1] = regTable[ins.tokens[2]];
										break;
								case LI:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										ins.ins_code[1] = str2int(ins.tokens[2]);
										break;
								case BB: case BEQ: case BNE: case BGE: case BLE: case BGT: case BLT:
								case BEQZ: case BNEZ: case BLEZ: case BGEZ: case BGTZ: case BLTZ:
										if(ins.tokens.size() == 4) {
												ins.not_imm = ins.tokens[2][0] == '$';
												ins.ins_code[0] = regTable[ins.tokens[1]];

												if(ins.not_imm)
														ins.ins_code[1] = regTable[ins.tokens[2]];
												else
														ins.ins_code[1] = str2int(ins.tokens[2]);
												ins.ins_code[2] = lab2src[ins.tokens[3]];
										}
										else if(ins.tokens.size() == 3) {
												ins.ins_code[0] = regTable[ins.tokens[1]];
												ins.ins_code[2] = lab2src[ins.tokens[2]];
										}
										else if(ins.tokens.size() == 2) {
												ins.ins_code[2] = lab2src[ins.tokens[1]];
										}
										else {
												cerr << "B bug" << endl;
										}
										break;
								case J: case JAL:
										ins.ins_code[0] = lab2src[ins.tokens[1]];
										break;
								case JR: case JALR:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										break;
								case LA: case LB: case LH: case LW:
								case SB: case SH: case SW:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										ins.ch_in_str = ch_in_string(ins.tokens[2], '$');
										if(!ins.ch_in_str)
												ins.ins_code[1] = var2mem[ins.tokens[2]];
										else
												displacement_extractor(regTable, ins.tokens[2], ins.ins_code[1], ins.ins_code[2]);
										break;
								case MOVE:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										ins.ins_code[1] = regTable[ins.tokens[2]];
										break;
								case MFHI: case MFLO:
										ins.ins_code[0] = regTable[ins.tokens[1]];
										break;
								case NOP:
										break;
								case SYSCALL:
										break;
								default:
										break;
						}
				}
    }
};

#endif //MYMIPSRE_PARSER_H
