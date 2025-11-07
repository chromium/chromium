// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/launch.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/child_process_binding_types.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/child_process_task_port_provider_mac.h"
#endif

namespace content {

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Returns the cumulative CPU usage for the specified process.
std::optional<base::TimeDelta> GetCPUUsage(base::ProcessHandle process_handle) {
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<base::ProcessMetrics> process_metrics =
      base::ProcessMetrics::CreateProcessMetrics(
          process_handle, ChildProcessTaskPortProvider::GetInstance());
#else
  std::unique_ptr<base::ProcessMetrics> process_metrics =
      base::ProcessMetrics::CreateProcessMetrics(process_handle);
#endif
  return base::OptionalFromExpected(process_metrics->GetCumulativeCPUUsage());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

using internal::ChildProcessLauncherHelper;

void RenderProcessPriority::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  // TODO(pmonette): Migrate is_background() to GetProcessPriority().
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
    case ChildProcessImportance::PERCEPTIBLE:
      proto->set_importance(PriorityProto::IMPORTANCE_PERCEPTIBLE);
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

bool ChildProcessLauncher::Client::HasSpareRendererPriority() {
  return false;
}
#endif

ChildProcessLauncher::ChildProcessLauncher(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> command_line,
    ChildProcessId child_process_id,
    Client* client,
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback,
    std::unique_ptr<ChildProcessLauncherFileData> file_data,
    scoped_refptr<base::RefCountedData<base::UnsafeSharedMemoryRegion>>
        histogram_memory_region,
    scoped_refptr<base::RefCountedData<base::ReadOnlySharedMemoryRegion>>
        tracing_config_memory_region,
    scoped_refptr<base::RefCountedData<base::UnsafeSharedMemoryRegion>>
        tracing_output_memory_region)
    : client_(client),
      starting_(true),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER) || BUILDFLAG(CLANG_PROFILING)
      terminate_child_on_shutdown_(false)
#else
      terminate_child_on_shutdown_(true)
#endif
{
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_BEGIN("startup", "ChildProcessLauncher",
                    perfetto::Track::FromPointer(this));

#if BUILDFLAG(IS_WIN)
  should_launch_elevated_ = delegate->ShouldLaunchElevated();
#endif

  helper_ = base::MakeRefCounted<ChildProcessLauncherHelper>(
      child_process_id, std::move(command_line), std::move(delegate),
      weak_factory_.GetWeakPtr(), terminate_child_on_shutdown_,
#if BUILDFLAG(IS_ANDROID)
      client_->CanUseWarmUpConnection(), client_->HasSpareRendererPriority(),
#endif
      std::move(mojo_invitation), process_error_callback, std::move(file_data),
      std::move(histogram_memory_region),
      std::move(tracing_config_memory_region),
      std::move(tracing_output_memory_region));
  helper_->StartLaunchOnClientThread();
}

ChildProcessLauncher::~ChildProcessLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (process_.process.IsValid() && terminate_child_on_shutdown_) {
    // Client has gone away, so just kill the process.
    ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
        std::move(process_));
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChildProcessLauncher::SetRenderProcessPriority(
    const RenderProcessPriority& priority) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Process to_pass = process_.process.Duplicate();
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::SetRenderProcessPriorityOnLauncherThread,
          helper_, std::move(to_pass), priority, base::TimeTicks::Now()));
}
#else   // !BUILDFLAG(IS_ANDROID)
void ChildProcessLauncher::SetProcessPriority(
    base::Process::Priority priority) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (priority == priority_) {
    return;
  }

  SetProcessPriorityImpl(priority);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ChildProcessLauncher::Notify(ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
                                  DWORD last_error,
#endif
                                  int error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Corresponds to the TRACE_EVENT_BEGIN in ChildProcessLauncher.
  TRACE_EVENT_END("startup", perfetto::Track::FromPointer(this));

  starting_ = false;
  process_ = std::move(process);

  if (process_.process.IsValid()) {
    process_start_time_ = base::TimeTicks::Now();

#if BUILDFLAG(IS_MAC)
    // On mac, the task port is required to change the priority of the child
    // process.
    auto* port_provider = ChildProcessTaskPortProvider::GetInstance();
    CHECK(port_provider);
    if (port_provider->TaskForHandle(process_.process.Handle()) ==
        MACH_PORT_NULL) {
      // In the most common case, the task port is not available at launch time.
      scoped_port_provider_observation_.Observe(port_provider);
    }
#endif

    // Note:: May delete |this|.
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

#if BUILDFLAG(IS_MAC)
void ChildProcessLauncher::OnReceivedTaskPort(
    base::ProcessHandle process_handle) {
  if (!process_.process.IsValid()) {
    // The process has died since. No need to keep observing for task ports.
    scoped_port_provider_observation_.Reset();
    return;
  }

  // Ignore notifications about different processes.
  if (process_.process.Handle() != process_handle) {
    return;
  }

  scoped_port_provider_observation_.Reset();

  if (priority_) {
    SetProcessPriorityImpl(*priority_);
  }
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void ChildProcessLauncher::SetProcessPriorityImpl(
    base::Process::Priority priority) {
  priority_ = priority;
  base::Process to_pass = process_.process.Duplicate();
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread,
          helper_, std::move(to_pass), priority));
}
#endif

bool ChildProcessLauncher::IsStarting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!starting_);
  return process_.process;
}

ChildProcessTerminationInfo ChildProcessLauncher::GetChildTerminationInfo(
    bool known_dead) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!process_.process.IsValid()) {
    // Make sure to avoid using the default termination status if the process
    // hasn't even started yet.
    if (IsStarting())
      termination_info_.status = base::TERMINATION_STATUS_STILL_RUNNING;

    // Process doesn't exist, so return the cached termination info.
    return termination_info_;
  }

#if !BUILDFLAG(IS_ANDROID)
  std::optional<base::TimeDelta> cpu_usage;
  if (!should_launch_elevated_)
    cpu_usage = GetCPUUsage(process_.process.Handle());
#endif

  termination_info_ = helper_->GetTerminationInfo(process_, known_dead);

#if !BUILDFLAG(IS_ANDROID)
  // Get the cumulative CPU usage. This needs to be done before closing the
  // process handle (on Windows) or reaping the zombie process (on MacOS, Linux,
  // ChromeOS).
  termination_info_.cpu_usage = cpu_usage;
#endif

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
base::android::ChildBindingState
ChildProcessLauncher::GetEffectiveChildBindingState() {
  return helper_->GetEffectiveChildBindingState();
}

void ChildProcessLauncher::DumpProcessStack() {
  base::Process to_pass = process_.process.Duplicate();
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ChildProcessLauncherHelper::DumpProcessStack,
                                helper_, std::move(to_pass)));
}

void ChildProcessLauncher::OnSpareRendererPriorityGraduated(bool is_alive) {
  client_->OnSpareRendererPriorityGraduated(is_alive);
}
#endif

ChildProcessLauncher::Client* ChildProcessLauncher::ReplaceClientForTest(
    Client* client) {
  Client* ret = client_;
  client_ = client;
  return ret;
}

RenderProcessPriority::RenderProcessPriority(bool visible,
                                             bool has_media_stream,
                                             bool has_immersive_xr_session,
                                             bool has_foreground_service_worker,
                                             unsigned int frame_depth,
                                             bool intersects_viewport,
                                             bool boost_for_pending_views,
                                             bool boost_for_loading,
                                             bool boost_for_discard,
#if BUILDFLAG(IS_ANDROID)
                                             bool is_spare_renderer,
                                             ChildProcessImportance importance
#else
                                             std::optional<
                                                 base::Process::Priority>
                                                 priority_override
#endif
                                             )
    : visible(visible),
      has_media_stream(has_media_stream),
      has_immersive_xr_session(has_immersive_xr_session),
      has_foreground_service_worker(has_foreground_service_worker),
      frame_depth(frame_depth),
      intersects_viewport(intersects_viewport),
      boost_for_pending_views(boost_for_pending_views),
      boost_for_loading(boost_for_loading),
      boost_for_discard(boost_for_discard),
#if BUILDFLAG(IS_ANDROID)
      is_spare_renderer(is_spare_renderer),
      importance(importance)
#endif
#if !BUILDFLAG(IS_ANDROID)
          priority_override(priority_override)
#endif
{
}

RenderProcessPriority::RenderProcessPriority(const RenderProcessPriority&) =
    default;

RenderProcessPriority& RenderProcessPriority::operator=(
    const RenderProcessPriority&) = default;

bool RenderProcessPriority::is_background() const {
#if !BUILDFLAG(IS_ANDROID)
  if (priority_override) {
    // TODO(pmonette): Migrate this logic to the performance manager's voting
    // system if it has a positive impact.
    if (base::FeatureList::IsEnabled(features::kPriorityOverridePendingViews) &&
        boost_for_pending_views) {
      return false;
    }
    // TODO(351953350): Migrate this logic to the performance manager.
    if (boost_for_loading || boost_for_discard) {
      return false;
    }
    return *priority_override == base::Process::Priority::kBestEffort;
  }
#endif
  return !visible && !has_media_stream && !has_immersive_xr_session &&
         !boost_for_pending_views && !has_foreground_service_worker &&
         !boost_for_loading && !boost_for_discard;
}

base::Process::Priority RenderProcessPriority::GetProcessPriority() const {
#if !BUILDFLAG(IS_ANDROID)
  if (priority_override) {
    // TODO(pmonette): Migrate this logic to the performance manager's voting
    // system if it has a positive impact.
    if (base::FeatureList::IsEnabled(features::kPriorityOverridePendingViews) &&
        boost_for_pending_views) {
      return base::Process::Priority::kUserBlocking;
    }
    // TODO(351953350): Migrate this logic to the performance manager.
    if (boost_for_loading || boost_for_discard) {
      return base::Process::Priority::kUserBlocking;
    }
    return *priority_override;
  }
#endif
  return is_background() ? base::Process::Priority::kBestEffort
                         : base::Process::Priority::kUserBlocking;
}

bool RenderProcessPriority::operator==(
    const RenderProcessPriority& other) const = default;

}  // namespace content
