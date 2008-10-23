// Copyright 2008 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
const char *help = "\
sae_load\n\
\n\
This program will load a model and test it on a dataset.\n\
\n";

#include <string>
#include <sstream>

#include "CmdLine.h"
#include "Allocator.h"
#include "ClassFormatDataSet.h"
#include "OneHotClassFormat.h"
#include "Measurer.h"
#include "MSEMeasurer.h"
#include "ClassMeasurer.h"
#include "ClassNLLMeasurer.h"
#include "MatDataSet.h"
#include "DiskXFile.h"
#include "helpers.h"


using namespace Torch;

// ************
// *** MAIN ***
// ************
int main(int argc, char **argv)
{

  // === The command-line ===

  int flag_n_inputs;
  int flag_n_classes;
  char *flag_model_filename;
  char *flag_testdata_filename;

  char *flag_expdir;
  char *flag_task;
  int flag_max_load;
  bool flag_binary_mode;

  // Construct the command line
  CmdLine cmd;

  // Put the help line at the beginning
  cmd.info(help);

  cmd.addText("\nArguments:");

  cmd.addICmdArg("-n_inputs", &flag_n_inputs, "number of inputs");
  cmd.addICmdArg("-n_classes", &flag_n_classes, "number of targets");
  cmd.addSCmdArg("-model_filename", &flag_model_filename, "the model filename");
  cmd.addSCmdArg("-testdata_filename", &flag_testdata_filename, "name of the test file");

  cmd.addText("\nOptions:");
  cmd.addSCmdOption("-expdir", &flag_expdir, "./", "Location where to write the expdir folder.", true);
  cmd.addSCmdOption("-task", &flag_task, "", "name of the task", true);
  cmd.addICmdOption("max_load", &flag_max_load, -1, "max number of examples to load for train", true);
  cmd.addBCmdOption("binary_mode", &flag_binary_mode, false, "binary mode for files", true);

  // Read the command line
  cmd.read(argc, argv);

  Allocator *allocator = new Allocator;

  std::string str_expdir = flag_expdir;
  if(str_expdir != "./")   {
    warning("Calling non portable mkdir!");
    std::stringstream command;
    command << "mkdir " << flag_expdir;
    system(command.str().c_str());
  }

  // data
  MatDataSet test_matdata(flag_testdata_filename, flag_n_inputs,1,false,
                                flag_max_load, flag_binary_mode);
  ClassFormatDataSet test_data(&test_matdata,flag_n_classes);
  OneHotClassFormat class_format(&test_data);   // Not sure about this... what if not all classes were in the test set?

  // model
  CommunicatingStackedAutoencoder *csae = LoadCSAE(allocator, flag_model_filename);

  // measurers
  MeasurerList measurers;

  std::stringstream measurer_filename;
  measurer_filename << flag_expdir << "/nll.txt";
  DiskXFile *file_nll = new(allocator) DiskXFile(measurer_filename.str().c_str(),"w");
  ClassNLLMeasurer *measurer_nll = new(allocator) ClassNLLMeasurer(csae->outputs, &test_data,
                                                                             &class_format, file_nll);
  measurers.addNode(measurer_nll);

  measurer_filename.str("");
  measurer_filename.clear();

  measurer_filename << flag_expdir << "/class.txt";
  DiskXFile *file_class = new(allocator) DiskXFile(measurer_filename.str().c_str(),"w");
  ClassMeasurer *measurer_class = new(allocator) ClassMeasurer(csae->outputs, &test_data,
                                                                         &class_format, file_class);
  measurers.addNode(measurer_class);

  // trainer
  StochasticGradient trainer(csae, NULL);
  trainer.test(&measurers);

  delete allocator;
  return(0);

}
