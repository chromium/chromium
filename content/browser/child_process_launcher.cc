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
#include "base/time/time.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/child_process_task_port_provider_mac.h"
#endif

namespace content {

using internal::ChildProcessLauncherHelper;

void ChildProcessLauncherPriority::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_is_backgrounded(is_background());
  proto->set_has_pending_views(boost_for_pending_views);

#if BUILDFLAG(IS_ANDROID)
  using PriorityProto = perfetto::protos::pbzero::ChildProcessLauncherPriority;
  switch (importance) {
    case ChildProcessImportance::IMPORTANT:
      proto->set_importance(PriorityProto::IMPORTANCE_IMPORTANT);
      break;
    case ChildProcessImportance::NORMAL:
      proto->set_importance(PriorityProto::IMPORTANCE_NORMAL);
      break;
    case ChildProcessImportance::MODERATE:
      proto->set_importance(PriorityProto::IMPORTANCE_MODERATE);
      break;
  }
#endif
}

ChildProcessLauncherFileData::ChildProcessLauncherFileData() = default;

ChildProcessLauncherFileData::~ChildProcessLauncherFileData() = default;

#if BUILDFLAG(IS_ANDROID)
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
    std::unique_ptr<ChildProcessLauncherFileData> file_data,
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
#if BUILDFLAG(IS_ANDROID)
      client_->CanUseWarmUpConnection(),
#endif
      std::move(mojo_invitation), process_error_callback, std::move(file_data));
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

void ChildProcessLauncher::Notify(ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
                                  DWORD last_error,
#endif
                                  int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  starting_ = false;
  process_ = std::move(process);

  if (process_.process.IsValid()) {
    process_start_time_ = base::TimeTicks::Now();
    client_->OnProcessLaunched();
  } else {
    termination_info_.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
    termination_info_.exit_code = error_code;
#if BUILDFLAG(IS_WIN)
    termination_info_.last_error = last_error;
#endif

    // NOTE: May delete |this|.
    client_->OnProcessLaunchFailed(error_code);
  }
}

bool ChildProcessLauncher::IsStarting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!starting_);
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

#if BUILDFLAG(IS_ANDROID)
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
  return !visible && !has_media_stream && !boost_for_pending_views &&
         !has_foreground_service_worker;
}

bool ChildProcessLauncherPriority::operator==(
    const ChildProcessLauncherPriority& other) const {
  return visible == other.visible &&
         has_media_stream == other.has_media_stream &&
         has_foreground_service_worker == other.has_foreground_service_worker &&
         frame_depth == other.frame_depth &&
         intersects_viewport == other.intersects_viewport &&
         boost_for_pending_views == other.boost_for_pending_views
#if BUILDFLAG(IS_ANDROID)
         && importance == other.importance
#endif
      ;
}

}  // namespace content
