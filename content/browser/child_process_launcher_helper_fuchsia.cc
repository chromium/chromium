// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {
namespace internal {

namespace {

const char* ProcessNameFromSandboxType(sandbox::mojom::Sandbox sandbox_type) {
  switch (sandbox_type) {
    case sandbox::mojom::Sandbox::kNoSandbox:
      return nullptr;
    case sandbox::mojom::Sandbox::kRenderer:
      return "renderer";
    case sandbox::mojom::Sandbox::kUtility:
      return "utility";
    case sandbox::mojom::Sandbox::kService:
      return "service";
    case sandbox::mojom::Sandbox::kServiceWithJit:
      return "service-with-jit";
    case sandbox::mojom::Sandbox::kGpu:
      return "gpu";
    case sandbox::mojom::Sandbox::kNetwork:
      return "network";
    case sandbox::mojom::Sandbox::kVideoCapture:
      return "video-capture";
    case sandbox::mojom::Sandbox::kAudio:
      return "audio";
    case sandbox::mojom::Sandbox::kCdm:
      return "cdm";
    case sandbox::mojom::Sandbox::kOnDeviceModelExecution:
      return "on-device-model-execution";
    case sandbox::mojom::Sandbox::kPrintCompositor:
      return "print-compositor";
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      return "speech-recognition";
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      return "print-backend";
#endif
  }
}

}  // namespace

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    base::Process::Priority priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // TODO(crbug.com/40611633): Fuchsia does not currently support this.
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
  return nullptr;
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return true;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  DCHECK(sandbox_policy_);

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
    const base::LaunchOptions* options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  DCHECK(mojo_channel_);
  DCHECK(mojo_channel_->remote_endpoint().is_valid());
  DCHECK(sandbox_policy_);

  Process child_process;
  // Move `sandbox_policy_` into the child process object so that it doesn't get
  // destroyed before the child process.
  child_process.sandbox_policy = std::move(sandbox_policy_);
  child_process.process = base::LaunchProcess(*command_line(), *options);
  return child_process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Wait for the process to terminate to ensure that `process` and its child
  // `sandbox_policy` aren't destroyed before the process is terminated.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, true);
}

}  // namespace internal
}  // namespace content
