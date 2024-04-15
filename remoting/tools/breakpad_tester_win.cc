// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdlib.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/logging.h"

namespace {

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const char kUsageMessage[] =
    "\n"
    "Usage: %s <pid>\n"
    "\n"
    "  pid  - PID of the process to be crashed.\n"
    "\n\n"
    "Note: You may need to run this tool as SYSTEM\n"
    "      to prevent access denied errors.\n";

// Exit codes:
const int kSuccessExitCode = 0;
const int kUsageExitCode = 1;
const int kErrorExitCode = 2;

void usage(const char* program_name) {
  fprintf(stderr, kUsageMessage, program_name);
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  base::AtExitManager exit_manager;

  remoting::InitHostLogging();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(argv[0]);
    return kSuccessExitCode;
  }

  base::CommandLine::StringVector args = command_line->GetArgs();
  if (args.size() != 1) {
    usage(argv[0]);
    return kUsageExitCode;
  }

  int pid = _wtoi(args[0].c_str());
  if (pid == 0) {
    LOG(ERROR) << "Invalid process PID: " << args[0];
    return kErrorExitCode;
  }

  DWORD desired_access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
  base::win::ScopedHandle process;
  process.Set(OpenProcess(desired_access, FALSE, pid));
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to open the process " << pid;
    return kErrorExitCode;
  }

  DWORD thread_id;
  base::win::ScopedHandle thread;
  thread.Set(CreateRemoteThread(process.Get(), NULL, 0, NULL, NULL, 0,
                                &thread_id));
  if (!thread.IsValid()) {
    PLOG(ERROR) << "Failed to create a remote thread in " << pid;
    return kErrorExitCode;
  }

  return kSuccessExitCode;
}
