// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init_win.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {
namespace internal {

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

absl::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  if (!delegate_->ShouldLaunchElevated())
    return absl::nullopt;

  mojo::NamedPlatformChannel::Options options;
  mojo::NamedPlatformChannel named_channel(options);
  named_channel.PassServerNameOnCommandLine(command_line());
  return named_channel;
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  return nullptr;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (delegate_->ShouldLaunchElevated()) {
    options->elevated = true;
  } else {
    mojo_channel_->PrepareToPassRemoteEndpoint(&options->handles_to_inherit,
                                               command_line());
  }
  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  *is_synchronous_launch = true;
  if (delegate_->ShouldLaunchElevated()) {
    DCHECK(options.elevated);
    // When establishing a Mojo connection, the pipe path has already been added
    // to the command line.
    base::LaunchOptions win_options;
    win_options.start_hidden = true;
    win_options.elevated = true;
    ChildProcessLauncherHelper::Process process;
    process.process = base::LaunchProcess(*command_line(), win_options);
    *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                               : LAUNCH_RESULT_FAILURE;
    return process;
  }
  ChildProcessLauncherHelper::Process process;
  *launch_result =
      StartSandboxedProcess(delegate_.get(), *command_line(),
                            options.handles_to_inherit, &process.process);
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  info.status =
      base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  return process.Terminate(exit_code, false);
}

void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
}

void ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread(
    base::Process process,
    bool is_background) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(is_background);
}

}  // namespace internal
}  // namespace content
