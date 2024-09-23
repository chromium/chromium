// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/sandbox_support_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace content {
namespace internal {

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return std::nullopt;
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return CreateDefaultPosixFilesToMap(
      child_process_id(), mojo_channel_->remote_endpoint(),
      file_data_->files_to_preload, GetProcessType(), command_line());
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return !GetZygoteForLaunch();
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  if (options) {
    DCHECK(!GetZygoteForLaunch());
    // Convert FD mapping to FileHandleMappingVector
    options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
        base::GlobalDescriptors::kBaseDescriptor);

    if (GetProcessType() == switches::kRendererProcess) {
      const int sandbox_fd = SandboxHostLinux::GetInstance()->GetChildSocket();
      options->fds_to_remap.emplace_back(sandbox_fd, GetSandboxFD());
    }

    options->environment = delegate_->GetEnvironment();
  } else {
    DCHECK(GetZygoteForLaunch());
    // Environment variables could be supported in the future, but are not
    // currently supported when launching with the zygote.
    DCHECK(delegate_->GetEnvironment().empty());
  }

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;
  Process process;
  ZygoteCommunication* zygote_handle = GetZygoteForLaunch();
  if (zygote_handle) {
    // TODO(crbug.com/40448989): If chrome supported multiple zygotes they could
    // be created lazily here, or in the delegate GetZygote() implementations.
    // Additionally, the delegate could provide a UseGenericZygote() method.
    base::ProcessHandle handle = zygote_handle->ForkRequest(
        command_line()->argv(), files_to_register->GetMapping(),
        GetProcessType());
    *launch_result = LAUNCH_RESULT_SUCCESS;

#if !BUILDFLAG(IS_OPENBSD)
    if (handle) {
      // It could be a renderer process or an utility process.
      int oom_score = content::kMiscOomScore;
      if (command_line()->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kRendererProcess)
        oom_score = content::kLowestRendererOomScore;
      ZygoteHostImpl::GetInstance()->AdjustRendererOOMScore(handle, oom_score);
    }
#endif

    process.process = base::Process(handle);
    process.zygote = zygote_handle;
  } else {
    process.process = base::LaunchProcess(*command_line(), *options);
    *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                               : LAUNCH_RESULT_FAILURE;
  }

#if BUILDFLAG(IS_CHROMEOS)
  process_id_ = process.process.Pid();
  if (GetProcessType() == switches::kRendererProcess ||
      base::FeatureList::IsEnabled(features::kSchedQoSOnResourcedForChrome)) {
    process.process.InitializePriority();
  }
#endif

  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  // Reset any FDs still held open.
  file_data_.reset();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  if (process.zygote) {
    info.status = process.zygote->GetTerminationStatus(
        process.process.Handle(), known_dead, &info.exit_code);
  } else if (known_dead) {
    info.status = base::GetKnownDeadTerminationStatus(process.process.Handle(),
                                                      &info.exit_code);
  } else {
    info.status =
        base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  }
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  // TODO(crbug.com/40565504): Determine whether we should also call
  // EnsureProcessTerminated() to make sure of process-exit, and reap it.
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  TRACE_EVENT0("chromeos",
               "ChildProcessLauncherHelper::ForceNormalProcessTerminationSync");
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  // On POSIX, we must additionally reap the child.
  if (process.zygote) {
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    process.zygote->EnsureProcessTerminated(process.process.Handle());
  } else {
    base::EnsureProcessTerminated(std::move(process.process));
  }
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    base::Process::Priority priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (process.CanSetPriority() && priority_ != priority) {
    priority_ = priority;
    process.SetPriority(priority);
  }
}

ZygoteCommunication* ChildProcessLauncherHelper::GetZygoteForLaunch() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoZygote)
             ? nullptr
             : delegate_->GetZygote();
}

base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  base::FilePath exe_dir;
  bool result = base::PathService::Get(base::BasePathKey::DIR_EXE, &exe_dir);
  DCHECK(result);
  base::File file(exe_dir.Append(path),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  *region = base::MemoryMappedFile::Region::kWholeFile;
  return file;
}

}  // namespace internal
}  // namespace content
