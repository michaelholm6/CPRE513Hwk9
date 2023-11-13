/**
 * @author Ashwin K J
 * @file
 */
#include "Diff_Mapping.hpp"
#include "Get_Input.hpp"
#include "Graph.hpp"
#include "Graph_Line.hpp"
#include "MVICFG.hpp"
#include "Module.hpp"
#include "Graph_Instruction.hpp"
#include <chrono>
#include <string>
#include <vector>

using namespace hydrogen_framework;

/**
 * Main function
 */
int main(int argc, char *argv[]) {
  /* Getting the input */
  if (argc < 2) {
    std::cerr << "Insufficient arguments\n"
              << "The correct format is as follows:\n"
              << "<Path-to-Module1> <Path-to-Module2> .. <Path-to-ModuleN> :: "
              << "<Path-to-file1-for-Module1> .. <Path-to-fileN-for-Module1> :: "
              << "<Path-to-file2-for-Module2> .. <Path-to-fileN-for-Module2> ..\n"
              << "Note that '::' is the demarcation\n";
    return 1;
  } // End check for min argument
  Hydrogen framework;
  if (!framework.validateInputs(argc, argv)) {
    return 2;
  } // End check for valid Input
  if (!framework.processInputs(argc, argv)) {
    return 3;
  } // End check for processing Inputs
  std::list<Module *> mod = framework.getModules();
  /* Create ICFG */
  unsigned graphVersion = 1;
  Module *firstMod = mod.front();
  Graph *MVICFG = buildICFG(firstMod, graphVersion);
  /* Start timer */
  auto mvicfgStart = std::chrono::high_resolution_clock::now();
  /* Create MVICFG */
  std::vector<std::vector<Graph_Instruction *>> deleted_path_list;
  std::vector<std::vector<Graph_Instruction *>> added_path_list;
  for (auto iterModule = mod.begin(), iterModuleEnd = mod.end(); iterModule != iterModuleEnd; ++iterModule) {
    auto iterModuleNext = std::next(iterModule);
    /* Proceed as long as there is a next module */
    if (iterModuleNext != iterModuleEnd) {
      /* Container for added and deleted MVICFG lines */
      std::list<Graph_Line *> addedLines;
      std::list<Graph_Line *> deletedLines;
      std::map<Graph_Line *, Graph_Line *> matchedLines; /**<Map From ICFG Graph_Line to MVICFG Graph_Line */
      std::list<Diff_Mapping> diffMap = generateLineMapping(*iterModule, *iterModuleNext);
      Graph *ICFG = buildICFG(*iterModuleNext, ++graphVersion);
      for (auto iter : diffMap) {
        /* iter.printFileInfo(); */
        std::list<Graph_Line *> iterAdd = addToMVICFG(MVICFG, ICFG, iter, graphVersion);
        std::list<Graph_Line *> iterDel = deleteFromMVICFG(MVICFG, ICFG, iter, graphVersion);
        for (auto line : iterAdd){
          std::vector<Graph_Instruction *> added_path;
          for (auto lineInst : line->getLineInstructions()) {
            added_path.push_back(lineInst);
          }
          added_path_list.push_back(added_path);
        }

        for (auto line : iterDel) {
          std::vector<Graph_Instruction *> deleted_path;
          for (auto lineInst : line->getLineInstructions()) {
            deleted_path.push_back(lineInst);
        }
        deleted_path_list.push_back(deleted_path);
        }
        for (int i = 0; i < deleted_path_list.size(); i++){
          while (getPredGivenGraphLine(deleted_path_list[i][0]->getGraphLine()).size()>=1){
              int added_path = 1;
              std::vector <Graph_Instruction *> original_path = deleted_path_list[i];
              for (Graph_Line* pred_graph_line : getPredGivenGraphLine(deleted_path_list[i][0]->getGraphLine())){
                std::list <Graph_Instruction *> new_graph_line_instructions = pred_graph_line->getLineInstructions();
                new_graph_line_instructions.reverse();
                std::vector <Graph_Instruction *> new_path = original_path;
                for (auto new_line_instruction : new_graph_line_instructions){
                new_path.insert(new_path.begin(), new_line_instruction); 
                }
                if (added_path > 1){
                deleted_path_list.push_back(new_path);
                }
                else{
                  deleted_path_list[i] = new_path;
                }
                added_path++;
              }
            }
          }
        for (int i = 0; i < deleted_path_list.size(); i++){
          while (getSuccGivenGraphLine(deleted_path_list[i].back()->getGraphLine()).size()>=1){
              int added_path = 1;
              std::vector <Graph_Instruction *> original_path = deleted_path_list[i];
              for (Graph_Line* succ_graph_line : getSuccGivenGraphLine(deleted_path_list[i].back()->getGraphLine())){
                std::list <Graph_Instruction *> new_graph_line_instructions = succ_graph_line->getLineInstructions();
                std::vector <Graph_Instruction *> new_path = original_path;
                for (auto new_line_instruction : new_graph_line_instructions){
                new_path.insert(new_path.end(), new_line_instruction); 
                }
                if (added_path > 1){
                deleted_path_list.push_back(new_path);
                }
                else{
                  deleted_path_list[i] = new_path;
                }
                added_path++;
              }
            }
          }
        std::map<Graph_Line *, Graph_Line *> iterMatch = matchedInMVICFG(MVICFG, ICFG, iter, graphVersion);
        addedLines.insert(addedLines.end(), iterAdd.begin(), iterAdd.end());
        deletedLines.insert(deletedLines.end(), iterDel.begin(), iterDel.end());
        matchedLines.insert(iterMatch.begin(), iterMatch.end());
      } // End loop for diffMap
      /* Update Map Edges */
      getEdgesForAddedLines(MVICFG, ICFG, addedLines, diffMap, graphVersion);
      /* Update the matched lines to get new temporary variable mapping for old lines */
      updateMVICFGVersion(MVICFG, addedLines, deletedLines, diffMap, graphVersion);
      /* Update Map Version */
      MVICFG->setGraphVersion(graphVersion);
    } // End check for iterModuleEnd
  } // End loop for Module
  /* Stop timer */
  auto mvicfgStop = std::chrono::high_resolution_clock::now();
  auto mvicfgBuildTime = std::chrono::duration_cast<std::chrono::milliseconds>(mvicfgStop - mvicfgStart);
  MVICFG->printGraph("MVICFG");
  std::cout << "Finished Building MVICFG in " << mvicfgBuildTime.count() << "ms\n";
  int path_number = 1;
  std::cout << "Newly removed paths: \n";
  for (std::vector <Graph_Instruction *> path : deleted_path_list){
    std::cout << "Path " + std::to_string(path_number) + ": ";
    for (Graph_Instruction *Instr : path){
      std::cout << std::to_string(Instr->getInstructionID()) + " ";
    }
    std::cout << "\n";
    path_number++;
  }
  std::cout << "Number of newly removed paths: " + std::to_string(path_number-1) + "\n";

  std::cout << "Newly added paths: \n";
  for (std::vector<Graph_Instruction *> path : added_path_list) {
    std::cout << "Path " + std::to_string(path_number) + ": ";
    for (Graph_Instruction *Instr : path) {
      std::cout << std::to_string(Instr->getInstructionID()) + " ";
    }
    std::cout << "\n";
    path_number++;
  }
  std::cout << "Number of newly added paths: " + std::to_string(path_number - 1) + "\n";

  /* Write output to file */
  std::ofstream rFile("Result.txt", std::ios::trunc);
  if (!rFile.is_open()) {
    std::cerr << "Unable to open file for printing the output\n";
    return 5;
  } // End check for Result file
  rFile << "Input Args:\n";
  for (auto i = 0; i < argc; ++ i) {
    rFile << argv[i] << "  ";
  } // End loop for writing arguments
  rFile << "\n";
  rFile << "Finished Building MVICFG in " << mvicfgBuildTime.count() << "ms\n";
  rFile.close();
  return 0;
} // End main