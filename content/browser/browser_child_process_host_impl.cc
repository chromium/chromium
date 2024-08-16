// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include <memory>

#include "base/base_switches.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/histogram_controller.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_host_impl.h"
#include "content/browser/metrics/histogram_shared_memory_config.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

#if BUILDFLAG(IS_APPLE)
#include "content/browser/child_process_task_port_provider_mac.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "services/tracing/public/cpp/system_tracing_service.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "content/public/common/profiling_utils.h"
#endif

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
#include "content/public/browser/browser_message_filter.h"
#endif

namespace content {
namespace {

static base::LazyInstance<
    BrowserChildProcessHostImpl::BrowserChildProcessList>::DestructorAtExit
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<BrowserChildProcessObserver>::Unchecked>::
    DestructorAtExit g_browser_child_process_observers =
        LAZY_INSTANCE_INITIALIZER;

void NotifyProcessLaunchedAndConnected(const ChildProcessData& data) {
  // Assert that the process is valid, as guaranteed in a comment on the
  // declaration of `BrowserChildProcessLaunchedAndConnected()`.
  CHECK(data.GetProcess().IsValid(), base::NotFatalUntil::M130);

  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessLaunchedAndConnected(data);
}

void NotifyProcessKilled(const ChildProcessData& data,
                         const ChildProcessTerminationInfo& info) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessKilled(data, info);
}

memory_instrumentation::mojom::ProcessType GetCoordinatorClientProcessType(
    ProcessType process_type) {
  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      return memory_instrumentation::mojom::ProcessType::RENDERER;
    case PROCESS_TYPE_UTILITY:
      return memory_instrumentation::mojom::ProcessType::UTILITY;
    case PROCESS_TYPE_GPU:
      return memory_instrumentation::mojom::ProcessType::GPU;
    case PROCESS_TYPE_PPAPI_PLUGIN:
    case PROCESS_TYPE_PPAPI_BROKER:
      return memory_instrumentation::mojom::ProcessType::PLUGIN;
    default:
      NOTREACHED_IN_MIGRATION();
      return memory_instrumentation::mojom::ProcessType::OTHER;
  }
}
void BindTracedProcessFromUIThread(
    base::WeakPtr<BrowserChildProcessHostImpl> weak_host,
    mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver) {
  if (!weak_host)
    return;

  weak_host->GetHost()->BindReceiver(std::move(receiver));
}

}  // namespace

// static
std::unique_ptr<BrowserChildProcessHost> BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    ChildProcessHost::IpcMode ipc_mode) {
  return std::make_unique<BrowserChildProcessHostImpl>(process_type, delegate,
                                                       ipc_mode);
}

BrowserChildProcessHost* BrowserChildProcessHost::FromID(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserChildProcessHostImpl::BrowserChildProcessList* process_list =
      g_child_process_list.Pointer();
  for (BrowserChildProcessHostImpl* host : *process_list) {
    if (host->GetData().id == child_process_id)
      return host;
  }
  return nullptr;
}

#if BUILDFLAG(IS_APPLE)
base::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return ChildProcessTaskPortProvider::GetInstance();
}
#endif

// static
BrowserChildProcessHostImpl::BrowserChildProcessList*
BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

// static
void BrowserChildProcessHostImpl::AddObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_browser_child_process_observers.Get().AddObserver(observer);
}

// static
void BrowserChildProcessHostImpl::RemoveObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_browser_child_process_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    ChildProcessHost::IpcMode ipc_mode)
    : data_(process_type), delegate_(delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  // Create a persistent memory segment for subprocess histograms.
  CreateMetricsAllocator();

  child_process_host_ = ChildProcessHost::Create(this, ipc_mode);

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  g_child_process_list.Get().remove(this);

  // Skip sending the disconnected notification if the connected notification
  // was never sent. The only exception here is when the main browser process
  // hosts the child, since InProcessUtilityThreadHelper still depends on this
  // behavior to know when the utility service was shut down.
  if (!launched_and_connected_ && !in_process_)
    return;

  if (launched_and_connected_ && !exited_abnormally_) {
    for (auto& observer : g_browser_child_process_observers.Get()) {
      observer.BrowserChildProcessExitedNormally(data_,
                                                 GetTerminationInfo(false));
    }
  }

  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessHostDisconnected(data_);
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (auto it = copy.begin(); it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

void BrowserChildProcessHostImpl::Launch(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    bool terminate_on_shutdown) {
  LaunchWithFileData(
      std::move(delegate), std::move(cmd_line),
      /*file_data=*/std::make_unique<ChildProcessLauncherFileData>(),
      terminate_on_shutdown);
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return child_process_host_.get();
}

const base::Process& BrowserChildProcessHostImpl::GetProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return data_.GetProcess();
}

std::unique_ptr<base::PersistentMemoryAllocator>
BrowserChildProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

void BrowserChildProcessHostImpl::SetName(const std::u16string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetMetricsName(
    const std::string& metrics_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  data_.metrics_name = metrics_name;
}

void BrowserChildProcessHostImpl::SetProcess(base::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!in_process_);

  // Only NaClProcessHost uses SetProcess(), and it always involve a legacy IPC
  // channel. The channel is never connected at the time of the call, so
  // NotifyProcessLaunchedAndConnected() never has to be invoked here.
  DCHECK(has_legacy_ipc_channel_ && !is_channel_connected_);

  DCHECK(!process.is_current());
  data_.SetProcess(std::move(process));
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
void BrowserChildProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  child_process_host_->AddFilter(filter->GetFilter());
}
#endif

void BrowserChildProcessHostImpl::LaunchWithFileData(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    std::unique_ptr<ChildProcessLauncherFileData> file_data,
    bool terminate_on_shutdown) {
  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line.get(),
                                                                data_.id);
  LaunchWithoutExtraCommandLineSwitches(
      std::move(delegate), std::move(cmd_line), std::move(file_data),
      terminate_on_shutdown);
}

void BrowserChildProcessHostImpl::LaunchWithoutExtraCommandLineSwitches(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    std::unique_ptr<ChildProcessLauncherFileData> file_data,
    bool terminate_on_shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!in_process_);

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
      switches::kDisableInProcessStackTraces,
      switches::kDisableBestEffortTasks,
      switches::kIPCConnectionTimeout,
      switches::kLogBestEffortTasks,
      switches::kPerfettoDisableInterning,
      switches::kTraceToConsole,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches);

  // All processes should have a non-empty metrics name.
  if (data_.metrics_name.empty())
    data_.metrics_name = GetProcessTypeNameInEnglish(data_.process_type);

  data_.sandbox_type = delegate->GetSandboxType();

  // Note that if this host has a legacy IPC Channel, we don't dispatch any
  // connection status notifications until we observe OnChannelConnected().
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  bool is_elevated = false;
#if BUILDFLAG(IS_WIN)
  is_elevated = (delegate->GetSandboxType() ==
                 sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges);
#endif
  if (!is_elevated)
    child_process_host_->SetProfilingFile(OpenProfilingFile());
#endif

  auto tracing_config_memory_region =
      tracing::CreateTracingConfigSharedMemory();

  child_process_launcher_ = std::make_unique<ChildProcessLauncher>(
      std::move(delegate), std::move(cmd_line), data_.id, this,
      std::move(*child_process_host_->GetMojoInvitation()),
      base::BindRepeating(&BrowserChildProcessHostImpl::OnMojoError,
                          weak_factory_.GetWeakPtr(),
                          base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(file_data), metrics_shared_region_.Duplicate(),
      std::move(tracing_config_memory_region), terminate_on_shutdown);
  ShareMetricsAllocatorToProcess();

  if (!has_legacy_ipc_channel_)
    OnProcessConnected();
}

#if !BUILDFLAG(IS_ANDROID)
void BrowserChildProcessHostImpl::SetProcessPriority(
    base::Process::Priority priority) {
  DCHECK(child_process_launcher_);
  DCHECK(!child_process_launcher_->IsStarting());
  child_process_launcher_->SetProcessPriority(priority);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
void BrowserChildProcessHostImpl::EnableWarmUpConnection() {
  can_use_warm_up_connection_ = true;
}

void BrowserChildProcessHostImpl::DumpProcessStack() {
  if (!child_process_launcher_) {
    return;
  }
  child_process_launcher_->DumpProcessStack();
}
#endif

ChildProcessTerminationInfo BrowserChildProcessHostImpl::GetTerminationInfo(
    bool known_dead) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!child_process_launcher_) {
    // If the delegate doesn't use Launch() helper.
    ChildProcessTerminationInfo info;
    // TODO(crbug.com/40255458): iOS is single process mode for now.
#if !BUILDFLAG(IS_IOS)
    info.status = base::GetTerminationStatus(data_.GetProcess().Handle(),
                                             &info.exit_code);
#endif
    return info;
  }
  return child_process_launcher_->GetChildTerminationInfo(known_dead);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(has_legacy_ipc_channel_);
  is_channel_connected_ = true;

  delegate_->OnChannelConnected(peer_pid);

  OnProcessConnected();
}

void BrowserChildProcessHostImpl::OnProcessConnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel or ChildProcessHost pipe.
  early_exit_watcher_.StopWatching();
#endif

  if (IsProcessLaunched()) {
    launched_and_connected_ = true;
    NotifyProcessLaunchedAndConnected(data_);
  }
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

void BrowserChildProcessHostImpl::OnBadMessageReceived(
    const IPC::Message& message) {
  std::string log_message = "Bad message received of type: ";
  if (message.IsValid()) {
    log_message += base::NumberToString(message.type());
  } else {
    log_message += "unknown";
  }
  TerminateOnBadMessageReceived(log_message);
}

void BrowserChildProcessHostImpl::BindChildHistogramFetcherFactory(
    mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
        factory) {
  GetHost()->BindReceiver(std::move(factory));
}

void BrowserChildProcessHostImpl::TerminateOnBadMessageReceived(
    const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create a memory dump. This will contain enough stack frames to work out
  // what the bad message was.
  base::debug::DumpWithoutCrashing();

  TerminateProcessForBadMessage(weak_factory_.GetWeakPtr(), error);
}

void BrowserChildProcessHostImpl::OnChannelInitialized(IPC::Channel* channel) {
  has_legacy_ipc_channel_ = true;

  // When using a legacy IPC Channel, we defer any notifications until the
  // Channel handshake is complete. See OnChannelConnected().
  is_channel_connected_ = false;
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  tracing_registration_.reset();

#if BUILDFLAG(IS_WIN)
  // OnChildDisconnected may be called without OnChannelConnected, so stop the
  // early exit watcher so GetTerminationStatus can close the process handle.
  early_exit_watcher_.StopWatching();
#endif

  if (child_process_launcher_.get() || IsProcessLaunched()) {
    ChildProcessTerminationInfo info =
        GetTerminationInfo(true /* known_dead */);
#if BUILDFLAG(IS_ANDROID)
    exited_abnormally_ = true;
    // Do not treat clean_exit, ie when child process exited due to quitting
    // its main loop, as a crash.
    if (!info.clean_exit) {
      delegate_->OnProcessCrashed(info.exit_code);
    }
    NotifyProcessKilled(data_, info);
#else  // BUILDFLAG(IS_ANDROID)
    switch (info.status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
        exited_abnormally_ = true;
        delegate_->OnProcessCrashed(info.exit_code);
        for (auto& observer : g_browser_child_process_observers.Get())
          observer.BrowserChildProcessCrashed(data_, info);
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
#if BUILDFLAG(IS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
        exited_abnormally_ = true;
        delegate_->OnProcessCrashed(info.exit_code);
        NotifyProcessKilled(data_, info);
        // Report that this child process was killed.
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
      case base::TERMINATION_STATUS_STILL_RUNNING: {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
      case base::TERMINATION_STATUS_LAUNCH_FAILED: {
        // This is handled in OnProcessLaunchFailed.
        NOTREACHED_IN_MIGRATION();
        break;
      }
      case base::TERMINATION_STATUS_NORMAL_TERMINATION: {
        // TODO(wfh): This should not be hit but is sometimes. Investigate.
        break;
      }
      case base::TERMINATION_STATUS_OOM: {
        // TODO(wfh): Decide to what to do with OOMs here.
        break;
      }
#if BUILDFLAG(IS_WIN)
      case base::TERMINATION_STATUS_INTEGRITY_FAILURE: {
        // TODO(wfh): Decide to what to do with CIG failures here.
        break;
      }
#endif  // BUILDFLAG(IS_WIN)
      case base::TERMINATION_STATUS_MAX_ENUM: {
        NOTREACHED_IN_MIGRATION();
        break;
      }
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  DCHECK(has_legacy_ipc_channel_);
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::CreateMetricsAllocator() {
  // Create a persistent memory segment for subprocess histograms only if
  // they're active in the browser.
  // TODO(crbug.com/40818143): Remove this.
  if (!base::GlobalHistogramAllocator::Get()) {
    DVLOG(1) << "GlobalHistogramAllocator not configured";
    return;
  }

  // This class is not expected to be used for renderer child processes.
  // TODO(crbug.com/40109064): CHECK, once proven that this scenario does not
  // occur in the wild, else remove dump and just return early if disproven.
  if (data_.process_type == PROCESS_TYPE_RENDERER) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  // Get the shared memory configuration for this process type, if any,
  auto shared_memory_config =
      GetHistogramSharedMemoryConfig(data_.process_type);
  if (!shared_memory_config.has_value()) {
    DVLOG(1) << "No histogram shared memory configured: " << "pid=" << data_.id
             << "; process_type='"
             << GetProcessTypeNameInEnglish(data_.process_type) << "'";
    return;
  }

  // Create the shared memory region and histogram allocator.
  auto shared_memory = base::HistogramSharedMemory::Create(
      data_.id, shared_memory_config.value());

  if (!shared_memory.has_value()) {
    DVLOG(1) << "Failed to create histogram shared memory for pid=" << data_.id
             << "; process_type='"
             << GetProcessTypeNameInEnglish(data_.process_type) << "'";
    return;
  }

  DVLOG(1) << "Createdhistogram shared memory for pid=" << data_.id
           << "; process_type='"
           << GetProcessTypeNameInEnglish(data_.process_type) << "'";

  metrics_shared_region_ = std::move(shared_memory->region);
  metrics_allocator_ = std::move(shared_memory->allocator);
}

void BrowserChildProcessHostImpl::ShareMetricsAllocatorToProcess() {
  // Only get histograms from content process types; skip "embedder" process
  // types.
  metrics::HistogramController::ChildProcessMode histogram_mode =
      data_.process_type >= PROCESS_TYPE_CONTENT_END
          ? metrics::HistogramController::ChildProcessMode::kPingOnly
          : metrics::HistogramController::ChildProcessMode::kGetHistogramData;

  if (metrics_allocator_) {
    metrics::HistogramController::GetInstance()->SetHistogramMemory(
        this, std::move(metrics_shared_region_), histogram_mode);
    DVLOG(1) << "metrics_shared_region_ has been moved: " << "pid=" << data_.id
             << "; process_type="
             << GetProcessTypeNameInEnglish(data_.process_type);
  } else {
    metrics::HistogramController::GetInstance()->SetHistogramMemory(
        this, base::UnsafeSharedMemoryRegion(), histogram_mode);
  }
}

void BrowserChildProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnProcessLaunchFailed(error_code);
  ChildProcessTerminationInfo info =
      child_process_launcher_->GetChildTerminationInfo(/*known_dead=*/true);
  DCHECK_EQ(info.status, base::TERMINATION_STATUS_LAUNCH_FAILED);

  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessLaunchFailed(data_, info);
  delete delegate_;  // Will delete us
}

#if BUILDFLAG(IS_ANDROID)
bool BrowserChildProcessHostImpl::CanUseWarmUpConnection() {
  return can_use_warm_up_connection_;
}
#endif

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::Process& process = child_process_launcher_->GetProcess();
  DCHECK(process.IsValid());

#if BUILDFLAG(IS_MAC)
  ChildProcessTaskPortProvider::GetInstance()->OnChildProcessLaunched(
      process.Pid(),
      static_cast<ChildProcessHostImpl*>(child_process_host_.get())
          ->child_process());
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  child_thread_type_switcher_.SetPid(process.Pid());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedObject());
  early_exit_watcher_.StartWatchingOnce(process.Handle(), this);
#endif

  DCHECK(!process.is_current());
  data_.SetProcess(process.Duplicate());
  delegate_->OnProcessLaunched();

  if (is_channel_connected_) {
    launched_and_connected_ = true;
    NotifyProcessLaunchedAndConnected(data_);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In ChromeOS, there are still child processes of NaCl modules, and they
  // don't contribute to tracing actually. So do not register those clients
  // to the tracing service. See https://crbug.com/1101468.
  if (data_.process_type >= PROCESS_TYPE_CONTENT_END)
    return;
#endif

  tracing_registration_ = TracingServiceController::Get().RegisterClient(
      process.Pid(), base::BindRepeating(&BindTracedProcessFromUIThread,
                                         weak_factory_.GetWeakPtr()));
  BackgroundTracingManagerImpl::ActivateForProcess(
      GetData().id,
      static_cast<ChildProcessHostImpl*>(GetHost())->child_process());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  system_tracing_service_ = std::make_unique<tracing::SystemTracingService>();
  child_process()->EnableSystemTracingService(
      system_tracing_service_->BindAndPassPendingRemote());
#endif
}

void BrowserChildProcessHostImpl::RegisterCoordinatorClient(
    mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
        client_process) {
  // Intentionally disallow non-browser processes from getting a Coordinator.
  receiver.reset();

  // The child process may have already terminated by the time this message is
  // dispatched. We do nothing in that case.
  if (!IsProcessLaunched())
    return;

  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::PendingReceiver<
                     memory_instrumentation::mojom::Coordinator> receiver,
                 mojo::PendingRemote<
                     memory_instrumentation::mojom::ClientProcess>
                     client_process,
                 memory_instrumentation::mojom::ProcessType process_type,
                 base::ProcessId process_id,
                 std::optional<std::string> service_name) {
                GetMemoryInstrumentationRegistry()->RegisterClientProcess(
                    std::move(receiver), std::move(client_process),
                    process_type, process_id, std::move(service_name));
              },
              std::move(receiver), std::move(client_process),
              GetCoordinatorClientProcessType(
                  static_cast<ProcessType>(data_.process_type)),
              child_process_launcher_->GetProcess().Pid(),
              delegate_->GetServiceName()));
}

bool BrowserChildProcessHostImpl::IsProcessLaunched() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return data_.GetProcess().IsValid();
}

// static
void BrowserChildProcessHostImpl::OnMojoError(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& error) {
  // Create a memory dump with the error message captured in a crash key value.
  // This will make it easy to determine details about what interface call
  // failed.
  //
  // It is important to call DumpWithoutCrashing synchronously - this will help
  // to preserve the callstack and the crash keys present when the bad mojo
  // message was received.
  mojo::debug::ScopedMessageErrorCrashKey scoped_error_key(error);
  base::debug::DumpWithoutCrashing();

  if (task_runner->BelongsToCurrentThread()) {
    TerminateProcessForBadMessage(process, error);
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserChildProcessHostImpl::TerminateProcessForBadMessage,
            process, error));
  }
}

// static
void BrowserChildProcessHostImpl::TerminateProcessForBadMessage(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    const std::string& error) {
  if (!process)
    return;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  DVLOG(1) << "Terminating child process for bad message: " << error;
  process->child_process_launcher_->Terminate(RESULT_CODE_KILLED_BAD_MESSAGE);
}

#if BUILDFLAG(IS_WIN)

void BrowserChildProcessHostImpl::OnObjectSignaled(HANDLE object) {
  OnChildDisconnected();
}

#endif

}  // namespace content
