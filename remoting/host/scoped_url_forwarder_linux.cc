// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/scoped_url_forwarder_linux.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/task/thread_pool.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

constexpr char kSetupUrlForwarderScriptFileName[] = "setup-url-forwarder";

void ExecuteSetupScriptWithSwitch(const std::string& switch_name) {
  base::FilePath setup_script_path;
  if (!base::PathService::Get(base::DIR_EXE, &setup_script_path)) {
    LOG(ERROR) << "Failed to get current directory.";
    return;
  }
  setup_script_path =
      setup_script_path.AppendASCII(kSetupUrlForwarderScriptFileName);
  base::CommandLine command(setup_script_path);
  command.AppendSwitch(switch_name);
  base::LaunchOptions launch_options;
  launch_options.wait = true;
  int exit_code = -1;
  base::LaunchProcess(command, launch_options).WaitForExit(&exit_code);
  if (exit_code != 0) {
    LOG(ERROR) << "Execution of the setup script with switch \"" << switch_name
               << "\" failed with exit code: " << exit_code;
  } else {
    HOST_LOG << "Execution of the setup script with switch \"" << switch_name
             << "\" succeeded";
  }
}

}  // namespace

ScopedUrlForwarderLinux::ScopedUrlForwarderLinux(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExecuteSetupScriptWithSwitch, "setup"));
}

ScopedUrlForwarderLinux::~ScopedUrlForwarderLinux() {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExecuteSetupScriptWithSwitch, "restore"));
}

// static
std::unique_ptr<ScopedUrlForwarder> ScopedUrlForwarder::Create() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      io_task_runner{
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})};
  return std::make_unique<ScopedUrlForwarderLinux>(*io_task_runner);
}

}  // namespace remoting
