// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/agent_integration_configurator_linux.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace remoting {

namespace {

base::FilePath GetBinaryPath() {
  base::FilePath binary_path;
  if (!base::PathService::Get(base::DIR_EXE, &binary_path)) {
    LOG(ERROR) << "Failed to get current executable directory.";
    return base::FilePath();
  }
  return binary_path.AppendASCII(AgentIntegrationConfigurator::kBinaryName);
}

bool CheckIsSupported() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath binary_path = GetBinaryPath();
  if (binary_path.empty()) {
    return false;
  }
  return access(binary_path.value().c_str(), X_OK) == 0;
}

bool RunCommandWithTimeout(const base::CommandLine& command,
                           base::TimeDelta timeout,
                           std::string* output = nullptr) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  base::LaunchOptions options;
  if (output) {
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
      PLOG(ERROR) << "Failed to create pipe for "
                  << command.GetCommandLineString();
      return false;
    }
    read_fd.reset(pipe_fds[0]);
    write_fd.reset(pipe_fds[1]);
    options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
  }

  base::Process process = base::LaunchProcess(command, options);
  write_fd.reset();
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch " << command.GetCommandLineString();
    return false;
  }

  // Waiting for the process to exit before reading `read_fd` is safe here
  // because the expected stdout output ("enabled" or "disabled") is strictly
  // well below the OS pipe buffer capacity (typically 64KB on Linux). If the
  // process misbehaves and blocks writing more data than the pipe buffer can
  // hold, `WaitForExitWithTimeout` will time out and terminate the process.
  int exit_code = -1;
  if (!process.WaitForExitWithTimeout(timeout, &exit_code)) {
    LOG(ERROR) << "Timed out waiting for " << command.GetCommandLineString();
    process.Terminate(/*exit_code=*/-1, /*wait=*/false);
    base::EnsureProcessTerminated(std::move(process));
    return false;
  }
  if (exit_code != 0) {
    LOG(ERROR) << command.GetCommandLineString()
               << " exited with code: " << exit_code;
    return false;
  }

  if (output) {
    output->clear();
    char buffer[64];
    ssize_t bytes_read =
        HANDLE_EINTR(read(read_fd.get(), buffer, sizeof(buffer)));
    if (bytes_read < 0) {
      PLOG(ERROR) << "Failed to read output from "
                  << command.GetCommandLineString();
      return false;
    }
    output->assign(buffer, static_cast<size_t>(bytes_read));
  }
  return true;
}

bool QueryAgentIntegrationStatus(base::TimeDelta timeout) {
  base::FilePath binary_path = GetBinaryPath();
  if (binary_path.empty()) {
    return false;
  }
  base::CommandLine command(binary_path);
  command.AppendArg("status");
  std::string output;
  if (!RunCommandWithTimeout(command, timeout, &output)) {
    return false;
  }
  std::string_view trimmed = base::TrimWhitespaceASCII(output, base::TRIM_ALL);
  if (trimmed == "enabled") {
    return true;
  }
  if (trimmed == "disabled") {
    return false;
  }
  LOG(ERROR) << "Unexpected output from " << command.GetCommandLineString()
             << ": " << trimmed;
  return false;
}

bool ExecuteSetAgentIntegrationState(bool enabled, base::TimeDelta timeout) {
  base::FilePath binary_path = GetBinaryPath();
  if (binary_path.empty()) {
    return false;
  }
  base::CommandLine command(binary_path);
  command.AppendArg(enabled ? "enable" : "disable");
  return RunCommandWithTimeout(command, timeout);
}

}  // namespace

AgentIntegrationConfiguratorLinux::AgentIntegrationConfiguratorLinux()
    : io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

AgentIntegrationConfiguratorLinux::~AgentIntegrationConfiguratorLinux() =
    default;

void AgentIntegrationConfiguratorLinux::IsAgentIntegrationEnabled(
    IsAgentIntegrationEnabledCallback callback) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&QueryAgentIntegrationStatus, command_timeout_),
      std::move(callback));
}

void AgentIntegrationConfiguratorLinux::SetAgentIntegrationEnabled(
    bool enabled,
    SetAgentIntegrationEnabledCallback callback) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ExecuteSetAgentIntegrationState, enabled,
                     command_timeout_),
      std::move(callback));
}

void AgentIntegrationConfiguratorLinux::SetCommandTimeoutForTesting(
    base::TimeDelta timeout) {
  command_timeout_ = timeout;
}

// static
void AgentIntegrationConfigurator::IsSupported(IsSupportedCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CheckIsSupported),
      std::move(callback));
}

// static
std::unique_ptr<AgentIntegrationConfigurator>
AgentIntegrationConfigurator::Create() {
  return std::make_unique<AgentIntegrationConfiguratorLinux>();
}

}  // namespace remoting
