// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace content {
namespace internal {

namespace {

const char* ProcessNameFromSandboxType(
    sandbox::policy::SandboxType sandbox_type) {
  switch (sandbox_type) {
    case sandbox::policy::SandboxType::kNoSandbox:
      return nullptr;
    case sandbox::policy::SandboxType::kWebContext:
      return "context";
    case sandbox::policy::SandboxType::kRenderer:
      return "renderer";
    case sandbox::policy::SandboxType::kUtility:
      return "utility";
    case sandbox::policy::SandboxType::kGpu:
      return "gpu";
    case sandbox::policy::SandboxType::kNetwork:
      return "network";
    case sandbox::policy::SandboxType::kVideoCapture:
      return "video-capture";
    default:
      NOTREACHED() << "Unknown sandbox_type.";
      return nullptr;
  }
}

}  // namespace

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // TODO(https://crbug.com/926583): Fuchsia does not currently support this.
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

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  sandbox_policy_ = std::make_unique<sandbox::policy::SandboxPolicyFuchsia>(
      delegate_->GetSandboxType());
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return std::unique_ptr<FileMappedForLaunch>();
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  mojo_channel_->PrepareToPassRemoteEndpoint(&options->handles_to_transfer,
                                             command_line());
  sandbox_policy_->UpdateLaunchOptionsForSandbox(options);

  // Set process name suffix to make it easier to identify the process.
  const char* process_type =
      ProcessNameFromSandboxType(delegate_->GetSandboxType());
  if (process_type)
    options->process_name_suffix = base::StringPrintf(":%s", process_type);

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  DCHECK(mojo_channel_);
  DCHECK(mojo_channel_->remote_endpoint().is_valid());

  Process child_process;
  child_process.process = base::LaunchProcess(*command_line(), options);
  return child_process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, true);
}

}  // namespace internal
}  // namespace content
