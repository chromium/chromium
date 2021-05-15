// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/child_process_task_port_provider_mac.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"

namespace content {
namespace internal {

absl::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  return absl::nullopt;
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return CreateDefaultPosixFilesToMap(
      child_process_id(), mojo_channel_->remote_endpoint(),
      /*files_to_preload=*/{}, GetProcessType(), command_line());
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector.
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);

  base::FieldTrialList::InsertFieldTrialHandleIfNeeded(
      &options->mach_ports_for_rendezvous);

  mojo::PlatformHandle endpoint =
      mojo_channel_->TakeRemoteEndpoint().TakePlatformHandle();
  DCHECK(endpoint.is_valid_mach_receive());
  options->mach_ports_for_rendezvous.insert(std::make_pair(
      'mojo', base::MachRendezvousPort(endpoint.TakeMachReceiveRight())));

  options->environment = delegate_->GetEnvironment();

  options->disclaim_responsibility = delegate_->DisclaimResponsibility();

  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*command_line_);

  bool no_sandbox =
      command_line_->HasSwitch(sandbox::policy::switches::kNoSandbox) ||
      sandbox::policy::IsUnsandboxedSandboxType(sandbox_type);

  if (!no_sandbox) {
    // Generate the profile string.
    std::string profile = sandbox::policy::GetSandboxProfile(sandbox_type);

    // Disable os logging to com.apple.diagnosticd which is a performance
    // problem.
    options->environment.insert(std::make_pair("OS_ACTIVITY_MODE", "disable"));

    seatbelt_exec_client_ = std::make_unique<sandbox::SeatbeltExecClient>();
    seatbelt_exec_client_->SetProfile(profile);

    SetupSandboxParameters(sandbox_type, *command_line_.get(),
                           seatbelt_exec_client_.get());

    int pipe = seatbelt_exec_client_->GetReadFD();
    if (pipe < 0) {
      LOG(ERROR) << "The file descriptor for the sandboxed child is invalid.";
      return false;
    }

    options->fds_to_remap.push_back(std::make_pair(pipe, pipe));

    // Update the command line to enable the V2 sandbox and pass the
    // communication FD to the helper executable.
    command_line_->AppendArg(
        base::StringPrintf("%s%d", sandbox::switches::kSeatbeltClient, pipe));
  }

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;
  ChildProcessLauncherHelper::Process process;
  process.process = base::LaunchProcess(*command_line(), options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
  // Send the sandbox profile after launch so that the child will exist and be
  // waiting for the message on its side of the pipe.
  if (process.process.IsValid() && seatbelt_exec_client_.get() != nullptr) {
    seatbelt_exec_client_->SendProfile();
  }
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  info.status = known_dead ? base::GetKnownDeadTerminationStatus(
                                 process.process.Handle(), &info.exit_code)
                           : base::GetTerminationStatus(
                                 process.process.Handle(), &info.exit_code);
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  // TODO(https://crbug.com/818244): Determine whether we should also call
  // EnsureProcessTerminated() to make sure of process-exit, and reap it.
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  base::EnsureProcessTerminated(std::move(process.process));
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  if (process.CanBackgroundProcesses()) {
    process.SetProcessBackgrounded(ChildProcessTaskPortProvider::GetInstance(),
                                   priority.is_background());
  }
}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  // Not used yet (until required files are described in the service manifest on
  // Mac).
  NOTREACHED();
  return base::File();
}

}  //  namespace internal
}  // namespace content
