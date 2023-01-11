// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/url_forwarder_configurator_linux.h"

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/task/thread_pool.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

constexpr char kConfigureUrlForwarderScriptFileName[] =
    "configure-url-forwarder";

// Returns true if the execution was successful (exit code is 0).
bool ExecuteConfigScriptWithSwitch(const std::string& switch_name) {
  base::FilePath setup_script_path;
  if (!base::PathService::Get(base::DIR_EXE, &setup_script_path)) {
    LOG(ERROR) << "Failed to get current directory.";
    return -1;
  }
  setup_script_path =
      setup_script_path.AppendASCII(kConfigureUrlForwarderScriptFileName);
  base::CommandLine command(setup_script_path);
  command.AppendSwitch(switch_name);
  int exit_code = -1;
  base::LaunchProcess(command, base::LaunchOptions()).WaitForExit(&exit_code);
  return exit_code == 0;
}

protocol::UrlForwarderControl::SetUpUrlForwarderResponse::State
SetUpForwarderAndGetResponseState() {
  return ExecuteConfigScriptWithSwitch("setup")
             ? protocol::UrlForwarderControl::SetUpUrlForwarderResponse::
                   COMPLETE
             : protocol::UrlForwarderControl::SetUpUrlForwarderResponse::FAILED;
}

}  // namespace

UrlForwarderConfiguratorLinux::UrlForwarderConfiguratorLinux()
    : io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

UrlForwarderConfiguratorLinux::~UrlForwarderConfiguratorLinux() = default;

void UrlForwarderConfiguratorLinux::IsUrlForwarderSetUp(
    IsUrlForwarderSetUpCallback callback) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ExecuteConfigScriptWithSwitch, "check-setup"),
      std::move(callback));
}

void UrlForwarderConfiguratorLinux::SetUpUrlForwarder(
    const SetUpUrlForwarderCallback& callback) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SetUpForwarderAndGetResponseState),
      base::BindOnce(callback));
}

// static
std::unique_ptr<UrlForwarderConfigurator> UrlForwarderConfigurator::Create() {
  return std::make_unique<UrlForwarderConfiguratorLinux>();
}

}  // namespace remoting
