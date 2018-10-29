// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/histogram_controller.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/service_manager/child_connection.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
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
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessLaunchedAndConnected(data);
}

void NotifyProcessHostConnected(const ChildProcessData& data) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessHostConnected(data);
}

void NotifyProcessHostDisconnected(const ChildProcessData& data) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessHostDisconnected(data);
}

#if !defined(OS_ANDROID)
void NotifyProcessCrashed(const ChildProcessData& data,
                          const ChildProcessTerminationInfo& info) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessCrashed(data, info);
}
#endif

void NotifyProcessKilled(const ChildProcessData& data,
                         const ChildProcessTerminationInfo& info) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessKilled(data, info);
}

}  // namespace

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate) {
  return Create(process_type, delegate, std::string());
}

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    const std::string& service_name) {
  return new BrowserChildProcessHostImpl(process_type, delegate, service_name);
}

BrowserChildProcessHost* BrowserChildProcessHost::FromID(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserChildProcessHostImpl::BrowserChildProcessList* process_list =
      g_child_process_list.Pointer();
  for (BrowserChildProcessHostImpl* host : *process_list) {
    if (host->GetData().id == child_process_id)
      return host;
  }
  return nullptr;
}

#if defined(OS_MACOSX)
base::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return MachBroker::GetInstance();
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
  // TODO(phajdan.jr): Check thread after fixing http://crbug.com/167126.
  g_browser_child_process_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    const std::string& service_name)
    : data_(process_type),
      delegate_(delegate),
      channel_(nullptr),
      is_channel_connected_(false),
      notify_child_disconnected_(false),
      weak_factory_(this) {
  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  child_process_host_.reset(ChildProcessHost::Create(this));
  AddFilter(new TraceMessageFilter(data_.id));

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);

  if (!service_name.empty()) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    service_manager::Identity child_identity(
        service_name, service_manager::mojom::kInheritUserID,
        base::StringPrintf("%d", data_.id));
    child_connection_.reset(
        new ChildConnection(child_identity, &mojo_invitation_,
                            ServiceManagerContext::GetConnectorForIOThread(),
                            base::ThreadTaskRunnerHandle::Get()));
    data_.metrics_name = service_name;
  }

  // Create a persistent memory segment for subprocess histograms.
  CreateMetricsAllocator();
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);

  if (notify_child_disconnected_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessHostDisconnected, data_.Duplicate()));
  }
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (auto it = copy.begin(); it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

// static
void BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(
    base::CommandLine* cmd_line) {
  // If we run base::FieldTrials, we want to pass to their state to the
  // child process so that it can act in accordance with each state.
  base::FieldTrialList::CopyFieldTrialStateToFlags(
      switches::kFieldTrialHandle, switches::kEnableFeatures,
      switches::kDisableFeatures, cmd_line);
}

// static
void BrowserChildProcessHostImpl::CopyTraceStartupFlags(
    base::CommandLine* cmd_line) {
  if (tracing::TraceStartupConfig::GetInstance()->IsEnabled()) {
    const auto trace_config =
        tracing::TraceStartupConfig::GetInstance()->GetTraceConfig();
    if (!trace_config.IsArgumentFilterEnabled()) {
      // The only trace option that we can pass through switches is the record
      // mode. Other trace options should have the default value.
      //
      // TODO(chiniforooshan): Add other trace options to switches if, for
      // example, they are used in a telemetry test that needs startup trace
      // events from renderer processes.
      cmd_line->AppendSwitchASCII(switches::kTraceStartup,
                                  trace_config.ToCategoryFilterString());
      cmd_line->AppendSwitchASCII(
          switches::kTraceStartupRecordMode,
          base::trace_event::TraceConfig::TraceRecordModeToStr(
              trace_config.GetTraceRecordMode()));
    }
  }
}

void BrowserChildProcessHostImpl::Launch(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    bool terminate_on_shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line.get(),
                                                                data_.id);

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
      service_manager::switches::kDisableInProcessStackTraces,
      switches::kDisableBackgroundTasks,
      switches::kDisableLogging,
      switches::kEnableLogging,
      switches::kIPCConnectionTimeout,
      switches::kLoggingLevel,
      switches::kTraceToConsole,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches,
                             arraysize(kForwardSwitches));

  if (child_connection_) {
    cmd_line->AppendSwitchASCII(
        service_manager::switches::kServiceRequestChannelToken,
        child_connection_->service_token());
  }

  // All processes should have a non-empty metrics name.
  DCHECK(!data_.metrics_name.empty());

  notify_child_disconnected_ = true;
  child_process_.reset(new ChildProcessLauncher(
      std::move(delegate), std::move(cmd_line), data_.id, this,
      std::move(mojo_invitation_),
      base::Bind(&BrowserChildProcessHostImpl::OnMojoError,
                 weak_factory_.GetWeakPtr(),
                 base::ThreadTaskRunnerHandle::Get()),
      terminate_on_shutdown));
  ShareMetricsAllocatorToProcess();
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return child_process_host_.get();
}

const base::Process& BrowserChildProcessHostImpl::GetProcess() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetProcess().IsValid())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetProcess();
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
BrowserChildProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

void BrowserChildProcessHostImpl::SetName(const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetMetricsName(
    const std::string& metrics_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.metrics_name = metrics_name;
}

void BrowserChildProcessHostImpl::SetHandle(base::ProcessHandle handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.SetHandle(handle);
}

service_manager::mojom::ServiceRequest
BrowserChildProcessHostImpl::TakeInProcessServiceRequest() {
  auto invitation = std::move(mojo_invitation_);
  return service_manager::mojom::ServiceRequest(
      invitation.ExtractMessagePipe(child_connection_->service_token()));
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

void BrowserChildProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  child_process_host_->AddFilter(filter->GetFilter());
}

void BrowserChildProcessHostImpl::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!child_connection_)
    return;

  child_connection_->BindInterface(interface_name, std::move(interface_pipe));
}

void BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
    ProcessType process_type) {
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.BadMessgeTerminated", process_type,
                            PROCESS_TYPE_MAX);
}

ChildProcessTerminationInfo BrowserChildProcessHostImpl::GetTerminationInfo(
    bool known_dead) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!child_process_) {
    // If the delegate doesn't use Launch() helper.
    ChildProcessTerminationInfo info;
    info.status =
        base::GetTerminationStatus(data_.GetHandle(), &info.exit_code);
    return info;
  }
  return child_process_->GetChildTerminationInfo(known_dead);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  is_channel_connected_ = true;
  notify_child_disconnected_ = true;

#if defined(OS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel.
  early_exit_watcher_.StopWatching();
#endif

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyProcessHostConnected, data_.Duplicate()));

  delegate_->OnChannelConnected(peer_pid);

  if (IsProcessLaunched()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessLaunchedAndConnected, data_.Duplicate()));
  }
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

void BrowserChildProcessHostImpl::OnBadMessageReceived(
    const IPC::Message& message) {
  std::string log_message = "Bad message received of type: ";
  if (message.IsValid()) {
    log_message += std::to_string(message.type());
  } else {
    log_message += "unknown";
  }
  TerminateOnBadMessageReceived(log_message);
}

void BrowserChildProcessHostImpl::TerminateOnBadMessageReceived(
    const std::string& error) {
  HistogramBadMessageTerminated(static_cast<ProcessType>(data_.process_type));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad IPC message: " << error;
  // Create a memory dump. This will contain enough stack frames to work out
  // what the bad message was.
  base::debug::DumpWithoutCrashing();

  child_process_->Terminate(RESULT_CODE_KILLED_BAD_MESSAGE);
}

void BrowserChildProcessHostImpl::OnChannelInitialized(IPC::Channel* channel) {
  channel_ = channel;
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_WIN)
  // OnChildDisconnected may be called without OnChannelConnected, so stop the
  // early exit watcher so GetTerminationStatus can close the process handle.
  early_exit_watcher_.StopWatching();
#endif
  if (child_process_.get() || data_.GetHandle()) {
    ChildProcessTerminationInfo info =
        GetTerminationInfo(true /* known_dead */);
#if defined(OS_ANDROID)
    delegate_->OnProcessCrashed(info.exit_code);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessKilled, data_.Duplicate(), info));
#else  // OS_ANDROID
    switch (info.status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
        delegate_->OnProcessCrashed(info.exit_code);
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&NotifyProcessCrashed, data_.Duplicate(), info));
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
        delegate_->OnProcessCrashed(info.exit_code);
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&NotifyProcessKilled, data_.Duplicate(), info));
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
      default:
        break;
    }
#endif  // OS_ANDROID
    UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected2",
                              static_cast<ProcessType>(data_.process_type),
                              PROCESS_TYPE_MAX);
#if defined(OS_CHROMEOS)
    if (info.status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM) {
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2.OOM",
                                static_cast<ProcessType>(data_.process_type),
                                PROCESS_TYPE_MAX);
    }
#endif
  }
  channel_ = nullptr;
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::CreateMetricsAllocator() {
  // Create a persistent memory segment for subprocess histograms only if
  // they're active in the browser.
  // TODO(bcwhite): Remove this once persistence is always enabled.
  if (!base::GlobalHistogramAllocator::Get())
    return;

  // Determine the correct parameters based on the process type.
  size_t memory_size;
  base::StringPiece metrics_name;
  switch (data_.process_type) {
    case PROCESS_TYPE_UTILITY:
      // This needs to be larger for the network service.
      memory_size = 256 << 10;  // 256 KiB
      metrics_name = "UtilityMetrics";
      break;

    case PROCESS_TYPE_ZYGOTE:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "ZygoteMetrics";
      break;

    case PROCESS_TYPE_SANDBOX_HELPER:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "SandboxHelperMetrics";
      break;

    case PROCESS_TYPE_GPU:
      // This needs to be larger for the display-compositor in the gpu process.
      memory_size = 256 << 10;  // 256 KiB
      metrics_name = "GpuMetrics";
      break;

    case PROCESS_TYPE_PPAPI_PLUGIN:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "PpapiPluginMetrics";
      break;

    case PROCESS_TYPE_PPAPI_BROKER:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "PpapiBrokerMetrics";
      break;

    default:
      // Report new processes. "Custom" ones are renumbered to 1000+ so that
      // they won't conflict with any standard ones in the future.
      int process_type = data_.process_type;
      if (process_type >= PROCESS_TYPE_CONTENT_END)
        process_type += 1000 - PROCESS_TYPE_CONTENT_END;
      base::UmaHistogramSparse(
          "UMA.SubprocessMetricsProvider.UntrackedProcesses", process_type);
      return;
  }

  // Create the shared memory segment and attach an allocator to it.
  // Mapping the memory shouldn't fail but be safe if it does; everything
  // will continue to work but just as if persistence weren't available.
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAndMapAnonymous(memory_size))
    return;
  metrics_allocator_.reset(new base::SharedPersistentMemoryAllocator(
      std::move(shm), static_cast<uint64_t>(data_.id), metrics_name,
      /*readonly=*/false));
}

void BrowserChildProcessHostImpl::ShareMetricsAllocatorToProcess() {
  if (metrics_allocator_) {
    HistogramController::GetInstance()->SetHistogramMemory<ChildProcessHost>(
        GetHost(),
        mojo::WrapSharedMemoryHandle(
            metrics_allocator_->shared_memory()->handle().Duplicate(),
            metrics_allocator_->shared_memory()->mapped_size(),
            mojo::UnwrappedSharedMemoryHandleProtection::kReadWrite));
  } else {
    HistogramController::GetInstance()->SetHistogramMemory<ChildProcessHost>(
        GetHost(), mojo::ScopedSharedBufferHandle());
  }
}

void BrowserChildProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  delegate_->OnProcessLaunchFailed(error_code);
  notify_child_disconnected_ = false;
  delete delegate_;  // Will delete us
}

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::Process& process = child_process_->GetProcess();
  DCHECK(process.IsValid());

  if (child_connection_)
    child_connection_->SetProcessHandle(process.Handle());

#if defined(OS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedObject());
  early_exit_watcher_.StartWatchingOnce(process.Handle(), this);
#endif

  data_.SetHandle(process.Handle());
  delegate_->OnProcessLaunched();

  if (is_channel_connected_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessLaunchedAndConnected, data_.Duplicate()));
  }
}

bool BrowserChildProcessHostImpl::IsProcessLaunched() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return child_process_.get() && child_process_->GetProcess().IsValid();
}

// static
void BrowserChildProcessHostImpl::OnMojoError(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& error) {
  if (!task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&BrowserChildProcessHostImpl::OnMojoError,
                                  process, task_runner, error));
    return;
  }
  if (!process)
    return;
  HistogramBadMessageTerminated(
      static_cast<ProcessType>(process->data_.process_type));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad Mojo message: " << error;

  // Create a memory dump with the error message captured in a crash key value.
  // This will make it easy to determine details about what interface call
  // failed.
  base::debug::ScopedCrashKeyString scoped_error_key(
      bad_message::GetMojoErrorCrashKey(), error);
  base::debug::DumpWithoutCrashing();
  process->child_process_->Terminate(RESULT_CODE_KILLED_BAD_MESSAGE);
}

#if defined(OS_WIN)

void BrowserChildProcessHostImpl::OnObjectSignaled(HANDLE object) {
  OnChildDisconnected();
}

#endif

}  // namespace content
