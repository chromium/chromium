// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace content {

using internal::ChildProcessLauncherHelper;

#if defined(OS_ANDROID)
bool ChildProcessLauncher::Client::CanUseWarmUpConnection() {
  return true;
}
#endif

ChildProcessLauncher::ChildProcessLauncher(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> command_line,
    int child_process_id,
    Client* client,
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback,
    std::map<std::string, base::FilePath> files_to_preload,
    bool terminate_on_shutdown)
    : client_(client),
      starting_(true),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER) || BUILDFLAG(CLANG_PROFILING)
      terminate_child_on_shutdown_(false)
#else
      terminate_child_on_shutdown_(terminate_on_shutdown)
#endif
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  helper_ = base::MakeRefCounted<ChildProcessLauncherHelper>(
      child_process_id, std::move(command_line), std::move(delegate),
      weak_factory_.GetWeakPtr(), terminate_on_shutdown,
#if defined(OS_ANDROID)
      client_->CanUseWarmUpConnection(),
#endif
      std::move(mojo_invitation), process_error_callback,
      std::move(files_to_preload));
  helper_->StartLaunchOnClientThread();
}

ChildProcessLauncher::~ChildProcessLauncher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_.process.IsValid() && terminate_child_on_shutdown_) {
    // Client has gone away, so just kill the process.
    ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
        std::move(process_));
  }
}

void ChildProcessLauncher::SetProcessPriority(
    const ChildProcessLauncherPriority& priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Process to_pass = process_.process.Duplicate();
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread,
          helper_, std::move(to_pass), priority));
}

void ChildProcessLauncher::Notify(
    ChildProcessLauncherHelper::Process process,
    int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  starting_ = false;
  process_ = std::move(process);

  if (process_.process.IsValid()) {
    client_->OnProcessLaunched();
  } else {
    termination_info_.status = base::TERMINATION_STATUS_LAUNCH_FAILED;

    // NOTE: May delete |this|.
    client_->OnProcessLaunchFailed(error_code);
  }
}

bool ChildProcessLauncher::IsStarting() {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_.process;
}

ChildProcessTerminationInfo ChildProcessLauncher::GetChildTerminationInfo(
    bool known_dead) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!process_.process.IsValid()) {
    // Make sure to avoid using the default termination status if the process
    // hasn't even started yet.
    if (IsStarting())
      termination_info_.status = base::TERMINATION_STATUS_STILL_RUNNING;

    // Process doesn't exist, so return the cached termination info.
    return termination_info_;
  }

  termination_info_ = helper_->GetTerminationInfo(process_, known_dead);

  // POSIX: If the process crashed, then the kernel closed the socket for it and
  // so the child has already died by the time we get here. Since
  // GetTerminationInfo called waitpid with WNOHANG, it'll reap the process.
  // However, if GetTerminationInfo didn't reap the child (because it was
  // still running), we'll need to Terminate via ProcessWatcher. So we can't
  // close the handle here.
  if (termination_info_.status != base::TERMINATION_STATUS_STILL_RUNNING) {
    process_.process.Exited(termination_info_.exit_code);
    process_.process.Close();
  }

  return termination_info_;
}

bool ChildProcessLauncher::Terminate(int exit_code) {
  return IsStarting() ? false
                      : ChildProcessLauncherHelper::TerminateProcess(
                            GetProcess(), exit_code);
}

// static
bool ChildProcessLauncher::TerminateProcess(const base::Process& process,
                                            int exit_code) {
  return ChildProcessLauncherHelper::TerminateProcess(process, exit_code);
}

#if defined(OS_ANDROID)
void ChildProcessLauncher::DumpProcessStack() {
  base::Process to_pass = process_.process.Duplicate();
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ChildProcessLauncherHelper::DumpProcessStack,
                                helper_, std::move(to_pass)));
}
#endif

ChildProcessLauncher::Client* ChildProcessLauncher::ReplaceClientForTest(
    Client* client) {
  Client* ret = client_;
  client_ = client;
  return ret;
}

bool ChildProcessLauncherPriority::is_background() const {
  if (boost_for_pending_views || has_foreground_service_worker ||
      has_media_stream) {
    return false;
  }
  return has_only_low_priority_frames || !visible;
}

bool ChildProcessLauncherPriority::operator==(
    const ChildProcessLauncherPriority& other) const {
  return visible == other.visible &&
         has_media_stream == other.has_media_stream &&
         has_foreground_service_worker == other.has_foreground_service_worker &&
         has_only_low_priority_frames == other.has_only_low_priority_frames &&
         frame_depth == other.frame_depth &&
         intersects_viewport == other.intersects_viewport &&
         boost_for_pending_views == other.boost_for_pending_views
#if defined(OS_ANDROID)
         && importance == other.importance
#endif
      ;
}

}  // namespace content
