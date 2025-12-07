// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_thread_impl.h"

#include <signal.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/base_switches.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/profiler.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/browser_exposed_child_interfaces.h"
#include "content/child/child_performance_coordinator.h"
#include "content/child/child_process.h"
#include "content/child/child_process_synthetic_trial_syncer.h"
#include "content/child/memory_coordinator/child_memory_consumer_registry.h"
#include "content/common/child_process.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/features.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_impl.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_provider_impl.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#if !BUILDFLAG(IS_ANDROID)
#include "services/tracing/public/cpp/system_tracing_service.h"
#include "services/tracing/public/cpp/traced_process.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_APPLE)
#include "base/apple/mach_port_rendezvous.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include <stdio.h>
#include "base/test/clang_profiling.h"
#include "build/config/compiler/compiler_buildflags.h"
#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#endif
#if BUILDFLAG(IS_WIN)
#include <io.h>
#endif
// Function provided by libclang_rt.profile-*.a, declared and documented at:
// https://github.com/llvm/llvm-project/blob/master/compiler-rt/lib/profile/InstrProfiling.h
extern "C" void __llvm_profile_set_file_object(FILE* File, int EnableMerge);
#endif

namespace content {
namespace {

// How long to wait for a connection to the browser process before giving up.
const int kConnectionTimeoutS = 15;

constinit thread_local ChildThreadImpl* child_thread_impl = nullptr;

// This isn't needed on Windows because there the sandbox's job object
// terminates child processes automatically. For unsandboxed processes (i.e.
// plugins), PluginThread has EnsureTerminateMessageFilter.
#if BUILDFLAG(IS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// A thread delegate that waits for |duration| and then exits the process
// immediately, without executing finalizers.
class WaitAndExitDelegate : public base::PlatformThread::Delegate {
 public:
  explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

  WaitAndExitDelegate(const WaitAndExitDelegate&) = delete;
  WaitAndExitDelegate& operator=(const WaitAndExitDelegate&) = delete;

  void ThreadMain() override {
    base::PlatformThread::Sleep(duration_);
    base::Process::TerminateCurrentProcessImmediately(0);
  }

 private:
  const base::TimeDelta duration_;
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelegate> delegate(
      new WaitAndExitDelegate(duration));

  const bool thread_created =
      base::PlatformThread::CreateNonJoinable(0, delegate.get());
  if (!thread_created)
    return false;

  // A non joinable thread has been created. The thread will either terminate
  // the process or will be terminated by the process. Therefore, keep the
  // delegate object alive for the lifetime of the process.
  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  std::ignore = leaking_delegate;
  return true;
}
#endif

void TerminateSelfOnDisconnect(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  // For renderer/worker processes:
  // On POSIX, at least, one can install an unload handler which loops
  // forever and leave behind a renderer process which eats 100% CPU forever.
  //
  // This is because the terminate signals (FrameMsg_BeforeUnload and the
  // error from the IPC sender) are routed to the main message loop but never
  // processed (because that message loop is stuck in V8).
  //
  // One could make the browser SIGKILL the renderers, but that leaves open a
  // large window where a browser failure (or a user, manually terminating
  // the browser because "it's stuck") will leave behind a process eating all
  // the CPU.
  //
  // So, we install a filter on the sender so that we can process this event
  // here and kill the process.
  base::debug::StopProfiling();
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
  // Some sanitizer tools rely on exit handlers (e.g. to run leak detection,
  // or dump code coverage data to disk). Instead of exiting the process
  // immediately, we give it 60 seconds to run exit handlers.
  CHECK(CreateWaitAndExitThread(base::Seconds(60)));
#if defined(LEAK_SANITIZER)
  // Invoke LeakSanitizer early to avoid detecting shutdown-only leaks. If
  // leaks are found, the process will exit here.
  __lsan_do_leak_check();
#endif
#else

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(CLANG_PROFILING)
  // TerminateSelfOnDisconnect() is called upon an IPC `OnChannelError`. Then,
  // clang will dump the profile to a file in
  // TerminateCurrentProcessImmediately. However, if the Android ActivityManager
  // detects the render thread as an 'isolated not needed' process, it sends
  // SIGKILL to this process, which corrupts the PGO profile. Here we call
  // `_exit()` without dumping the `clang` profile.
  _exit(0);
#else
  if (base::FeatureList::IsEnabled(features::kKeepChildProcessAfterIPCReset)) {
    // On Android, the browser process unbinds all service bindings to the child
    // process to terminate the child process and AMS (ActivityManagerService)
    // kills the child process. The child process should keep alive after IPC
    // disconnection. Otherwise AMS tries to restart the child process
    // immediately and kills it again when the service unbinding request
    // arrives.

    // The browser process must unbind all service bindings to the child process
    // to terminate the child process if the mojo connection is disconnected.
    // However, the child process may leak if mojo/RenderProcessHost have a bug.
    // Leaked child processes will crash so that we can notice the leak and the
    // bug.
    io_task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce([]() {
          LOG(FATAL) << "child process leaks after channel error.";
        }),
        base::Minutes(1));
    return;
  }

  base::Process::TerminateCurrentProcessImmediately(0);
#endif  // IS_ANDROID && CLANG_PROFILING
#endif
}

#endif  // OS(POSIX)

mojo::IncomingInvitation InitializeMojoIPCChannel() {
  TRACE_EVENT0("startup", "InitializeMojoIPCChannel");
  mojo::PlatformChannelEndpoint endpoint;
  MojoAcceptInvitationFlags flags =
      MOJO_ACCEPT_INVITATION_FLAG_LEAK_TRANSPORT_ENDPOINT;
#if BUILDFLAG(IS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          mojo::PlatformChannel::kHandleSwitch)) {
    endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
        *base::CommandLine::ForCurrentProcess());
  } else {
    // If this process is elevated, it will have a pipe path passed on the
    // command line.
    endpoint = mojo::NamedPlatformChannel::ConnectToServer(
        *base::CommandLine::ForCurrentProcess());
    flags |= MOJO_ACCEPT_INVITATION_FLAG_ELEVATED;
  }
#elif BUILDFLAG(IS_FUCHSIA)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_IOS_TVOS)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#else
  auto* client = base::MachPortRendezvousClient::GetInstance();
  if (!client) {
    LOG(ERROR) << "Mach rendezvous failed, terminating process (parent died?)";
    base::Process::TerminateCurrentProcessImmediately(0);
  }
  auto receive = client->TakeReceiveRight('mojo');
  if (!receive.is_valid()) {
    LOG(ERROR) << "Invalid PlatformChannel receive right";
    base::Process::TerminateCurrentProcessImmediately(0);
  }
  endpoint =
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(receive)));
#endif
#elif BUILDFLAG(IS_POSIX)
#if BUILDFLAG(IS_ANDROID)
  // If the endpoint is backed by a BinderRef it will be recovered here.
  // Otherwise we'll assume a socket FD below.
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#endif
  if (!endpoint.is_valid()) {
    endpoint =
        mojo::PlatformChannelEndpoint(mojo::PlatformHandle(base::ScopedFD(
            base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel))));
  }
#endif
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMojoBroker)) {
    flags |= MOJO_ACCEPT_INVITATION_FLAG_INHERIT_BROKER;
  }

  return mojo::IncomingInvitation::Accept(std::move(endpoint), flags);
}

// Callback passed to variations::ChildProcessFieldTrialSyncer. Notifies the
// browser process that a field trial group was activated in this process.
void FieldTrialActivatedCallback(
    mojo::SharedRemote<mojom::FieldTrialRecorder> recorder,
    const std::string& trial_name) {
  recorder->FieldTrialActivated(trial_name);
}

}  // namespace

// Implements the mojom ChildProcess interface and lives on the IO thread.
class ChildThreadImpl::IOThreadState
    : public base::RefCountedThreadSafe<IOThreadState>,
      public mojom::ChildProcess {
 public:
  IOThreadState(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      base::WeakPtr<ChildThreadImpl> weak_main_thread,
      base::RepeatingClosure quit_closure,
      ChildThreadImpl::Options::ServiceBinder service_binder)
      : main_thread_task_runner_(std::move(main_thread_task_runner)),
        weak_main_thread_(std::move(weak_main_thread)),
        quit_closure_(std::move(quit_closure)),
        service_binder_(std::move(service_binder)) {}

  IOThreadState(const IOThreadState&) = delete;
  IOThreadState& operator=(const IOThreadState&) = delete;

  // Used only in the deprecated Service Manager IPC mode.
  void BindChildProcessReceiver(
      mojo::PendingReceiver<mojom::ChildProcess> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void ExposeInterfacesToBrowser(mojo::BinderMap binders) {
    DCHECK(wait_for_interface_binders_);
    wait_for_interface_binders_ = false;
    interface_binders_ = std::move(binders);
    std::vector<mojo::GenericPendingReceiver> pending_requests;
    std::swap(pending_requests, pending_binding_requests_);
    for (auto& receiver : pending_requests)
      BindReceiver(std::move(receiver));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadState>;

  ~IOThreadState() override = default;

  // mojom::ChildProcess:
  void ProcessShutdown() override {
    main_thread_task_runner_->PostTask(FROM_HERE,
                                       base::BindOnce(quit_closure_));
  }

#if BUILDFLAG(IS_APPLE)
  void GetTaskPort(GetTaskPortCallback callback) override {
    mojo::PlatformHandle task_port(
        (base::apple::ScopedMachSendRight(task_self_trap())));
    std::move(callback).Run(std::move(task_port));
  }
#endif

  void GetBackgroundTracingAgentProvider(
      mojo::PendingReceiver<tracing::mojom::BackgroundTracingAgentProvider>
          receiver) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildThreadImpl::GetBackgroundTracingAgentProvider,
                       weak_main_thread_, std::move(receiver)));
  }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  void EnableSystemTracingService(
      mojo::PendingRemote<tracing::mojom::SystemTracingService> remote)
      override {
    tracing::TracedProcess::EnableSystemTracingService(std::move(remote));
  }
#endif

  // Make sure this isn't inlined, tail-called, or folded by ICF so it always
  // shows up in stack traces.
  NOT_TAIL_CALLED NOINLINE void CrashHungProcess() override {
    NO_CODE_FOLDING();
    LOG(FATAL) << "Crashing because hung";
  }

  void BindServiceInterface(mojo::GenericPendingReceiver receiver) override {
    if (service_binder_)
      service_binder_.Run(&receiver);

    if (receiver) {
      main_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChildThreadImpl::BindServiceInterface,
                                    weak_main_thread_, std::move(receiver)));
    }
  }

  void BindReceiver(mojo::GenericPendingReceiver receiver) override {
    if (wait_for_interface_binders_) {
      pending_binding_requests_.push_back(std::move(receiver));
      return;
    }

    if (interface_binders_.TryBind(&receiver))
      return;

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChildThreadImpl::OnBindReceiver,
                                  weak_main_thread_, std::move(receiver)));
  }

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void SetProfilingFile(base::File file) override {
    // If |file| is unused, its DTOR will call base::File::Close(), but this
    // would trigger DCHECK on the UI thread. Rejected fixes:
    // * Run on IO thread: This causes sequencing checker failure, whose fix
    //   might upend PGO use case.
    // * Use base::ScopedDisallowBlocking: This requires making
    //   ChildThreadImpl::IOThreadState a friend of the class. Unfortunately,
    //   inner classes cannot be forward declared.
    // The simple fix adopted is to explicitly close |file| if unused, thus
    // eliding checks -- this function is rather low-level anyway.
#if BUILDFLAG(IS_POSIX)
    // Take the file descriptor so that |file| does not close it.
    base::ScopedFD fd(file.TakePlatformFile());
#if BUILDFLAG(CLANG_PGO_PROFILING) || BUILDFLAG(USE_CLANG_COVERAGE)
    FILE* f = fdopen(fd.release(), "r+b");
    __llvm_profile_set_file_object(f, 1);
#else
    // Let |fd| close file descriptor.
#endif
#elif BUILDFLAG(IS_WIN)
    HANDLE handle = file.TakePlatformFile();
    int fd = _open_osfhandle((intptr_t)handle, 0);
    FILE* f = _fdopen(fd, "r+b");
    __llvm_profile_set_file_object(f, 1);
#else
#error Unsupported architecture for profiling.
#endif
  }

  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override {
    base::WriteClangProfilingProfile();
    std::move(callback).Run();
  }
#endif

  void SetPseudonymizationSalt(uint32_t salt) override {
    content::SetPseudonymizationSalt(salt);
  }

#if BUILDFLAG(IS_CHROMEOS)
  void ReinitializeLogging(mojom::LoggingSettingsPtr settings) override {
    logging::LoggingSettings logging_settings;
    logging_settings.logging_dest = settings->logging_dest;
    base::ScopedFD log_file_descriptor = settings->log_file_descriptor.TakeFD();
    logging_settings.log_file = fdopen(log_file_descriptor.release(), "a");
    if (!logging_settings.log_file) {
      LOG(ERROR) << "Failed to open new log file handle";
      return;
    }
    if (!logging::InitLogging(logging_settings))
      LOG(ERROR) << "Unable to reinitialize logging";
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  void OnMemoryPressure(base::MemoryPressureLevel level) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildThreadImpl::OnMemoryPressureFromBrowserReceived,
                       weak_main_thread_, level));
  }
#endif

  void SetBatterySaverMode(bool battery_saver_mode_enabled) override {
    if (battery_saver_mode_enabled) {
      if (base::FeatureList::IsEnabled(
              features::kBatterySaverModeAlignWakeUps)) {
        base::MessagePump::OverrideAlignWakeUpsState(true,
                                                     base::Milliseconds(32));
      }
    } else {
      base::MessagePump::ResetAlignWakeUpsState();
    }
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildThreadImpl::SetBatterySaverMode, weak_main_thread_,
                       battery_saver_mode_enabled));
  }

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const base::WeakPtr<ChildThreadImpl> weak_main_thread_;
  const base::RepeatingClosure quit_closure_;

  ChildThreadImpl::Options::ServiceBinder service_binder_;
  mojo::BinderMap interface_binders_;
  bool wait_for_interface_binders_ = true;
  mojo::Receiver<mojom::ChildProcess> receiver_{this};

  // Binding requests which should be handled by |interface_binders|, but which
  // have been queued because |allow_interface_binders_| is still |false|.
  std::vector<mojo::GenericPendingReceiver> pending_binding_requests_;
};

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

ChildThreadImpl::Options::Options() = default;

ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() = default;

ChildThreadImpl::Options::Builder::Builder() = default;

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(
    const InProcessChildThreadParams& params) {
  options_.browser_process_io_runner = params.io_runner();
  options_.mojo_invitation = params.mojo_invitation();
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::WithLegacyIPCChannel(bool with_channel) {
  options_.with_legacy_ipc_channel = with_channel;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ConnectToBrowser(
    bool connect_to_browser_parms) {
  options_.connect_to_browser = connect_to_browser_parms;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::IPCTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_parms) {
  options_.ipc_task_runner = ipc_task_runner_parms;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ServiceBinder(
    ChildThreadImpl::Options::ServiceBinder binder) {
  options_.service_binder = std::move(binder);
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ExposesInterfacesToBrowser() {
  options_.exposes_interfaces_to_browser = true;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::SetUrgentMessageObserver(
    IPC::UrgentMessageObserver* observer) {
  options_.urgent_message_observer = observer;
  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure), Options::Builder().Build()) {}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure,
                                 const Options& options)
    : resetter_(&child_thread_impl, this),
      quit_closure_(std::move(quit_closure)),
      browser_process_io_runner_(options.browser_process_io_runner),
      channel_connected_factory_(
          new base::WeakPtrFactory<ChildThreadImpl>(this)),
      ipc_task_runner_(options.ipc_task_runner) {
  io_thread_state_ = base::MakeRefCounted<IOThreadState>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      weak_factory_.GetWeakPtr(), quit_closure_,
      std::move(options.service_binder));

  // |ExposeInterfacesToBrowser()| must be called exactly once. Subclasses which
  // set |exposes_interfaces_to_browser| in Options signify that they take
  // responsibility for calling it.
  //
  // For other process types, we call it to expose only the basic set of
  // interfaces common to all child process types.
  if (!options.exposes_interfaces_to_browser)
    ExposeInterfacesToBrowser(mojo::BinderMap());

  Init(options);
}

scoped_refptr<base::SingleThreadTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInBrowserProcess())
    return browser_process_io_runner_;
  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::SetFieldTrialGroup(const std::string& trial_name,
                                         const std::string& group_name) {
  if (!field_trial_syncer_)
    return;

  field_trial_syncer_->SetFieldTrialGroupFromBrowser(trial_name, group_name);
}

void ChildThreadImpl::Init(const Options& options) {
  TRACE_EVENT0("startup", "ChildThreadImpl::Init");
  on_channel_error_called_ = false;
  main_thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  if (options.with_legacy_ipc_channel) {
    channel_ = IPC::SyncChannel::Create(
        this, ChildProcess::current()->io_task_runner(),
        ipc_task_runner_ ? ipc_task_runner_
                         : base::SingleThreadTaskRunner::GetCurrentDefault(),
        ChildProcess::current()->GetShutDownEvent());
    if (options.urgent_message_observer) {
      channel_->SetUrgentMessageObserver(options.urgent_message_observer);
    }
  }

  mojo::ScopedMessagePipeHandle child_process_pipe_for_receiver;
  mojo::ScopedMessagePipeHandle child_process_host_pipe_for_remote;
  mojo::ScopedMessagePipeHandle legacy_ipc_bootstrap_pipe;
  if (!IsInBrowserProcess()) {
    scoped_refptr<base::SingleThreadTaskRunner> mojo_ipc_task_runner =
        GetIOTaskRunner();
    if (base::FeatureList::IsEnabled(features::kMojoDedicatedThread)) {
      mojo_ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0));
      mojo_ipc_task_runner = mojo_ipc_thread_.task_runner();
    }
    mojo_ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        mojo_ipc_task_runner,
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

    mojo::IncomingInvitation invitation = InitializeMojoIPCChannel();
    if (!invitation.is_valid()) {
      LOG(ERROR) << "Child process could not find its Mojo invitation";
      base::Process::TerminateCurrentProcessImmediately(0);
    }
    child_process_pipe_for_receiver =
        invitation.ExtractMessagePipe(kChildProcessReceiverAttachmentName);
    child_process_host_pipe_for_remote =
        invitation.ExtractMessagePipe(kChildProcessHostRemoteAttachmentName);
    if (options.with_legacy_ipc_channel) {
      legacy_ipc_bootstrap_pipe =
          invitation.ExtractMessagePipe(kLegacyIpcBootstrapAttachmentName);
    }
  } else {
    child_process_pipe_for_receiver =
        options.mojo_invitation->ExtractMessagePipe(
            kChildProcessReceiverAttachmentName);
    child_process_host_pipe_for_remote =
        options.mojo_invitation->ExtractMessagePipe(
            kChildProcessHostRemoteAttachmentName);
    if (options.with_legacy_ipc_channel) {
      legacy_ipc_bootstrap_pipe = options.mojo_invitation->ExtractMessagePipe(
          kLegacyIpcBootstrapAttachmentName);
    }
  }

  // Now that we've recovered the message pipe for the ChildProcessHost, build
  // our |child_process_host_| with it.
  mojo::PendingRemote<mojom::ChildProcessHost> remote_host(
      std::move(child_process_host_pipe_for_remote), /*version=*/0u);
  child_process_host_ = mojo::SharedRemote<mojom::ChildProcessHost>(
      std::move(remote_host), GetIOTaskRunner());

  // In single process mode, browser-side tracing and memory will cover the
  // whole process including renderers.
  if (!IsInBrowserProcess()) {
    mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator;
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess> process;
    auto process_receiver = process.InitWithNewPipeAndPassReceiver();
    mojo::Remote<memory_instrumentation::mojom::CoordinatorConnector> connector;
    BindHostReceiver(connector.BindNewPipeAndPassReceiver());
    connector->RegisterCoordinatorClient(
        coordinator.InitWithNewPipeAndPassReceiver(), std::move(process));
    memory_instrumentation::ClientProcessImpl::CreateInstance(
        std::move(process_receiver), std::move(coordinator));
  }

  // In single process mode we may already have initialized the power monitor,
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      !power_monitor->IsInitialized()) {
    auto power_monitor_source =
        std::make_unique<device::PowerMonitorBroadcastSource>(
            GetIOTaskRunner());
    auto* source_ptr = power_monitor_source.get();
    power_monitor->Initialize(std::move(power_monitor_source));
    // The two-phase init is necessary to ensure that the process-wide
    // PowerMonitor is set before the power monitor source receives incoming
    // communication from the browser process (see https://crbug.com/821790 for
    // details)
    mojo::PendingRemote<device::mojom::PowerMonitor> remote_power_monitor;
    BindHostReceiver(remote_power_monitor.InitWithNewPipeAndPassReceiver());
    source_ptr->Init(std::move(remote_power_monitor));
  }

  performance_coordinator_ = std::make_unique<ChildPerformanceCoordinator>();
  BindHostReceiver(performance_coordinator_->InitializeAndPassReceiver());

  if (!IsInBrowserProcess()) {
    // Connect the global ChildMemoryConsumerRegistry with the browser registry.
    BindHostReceiver(ChildMemoryConsumerRegistry::BindAndPassReceiver());
  }

#if BUILDFLAG(IS_POSIX)
  // Check that --process-type is specified so we don't do this in unit tests
  // and single-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    child_process_host_.set_disconnect_handler(
        base::BindOnce(&TerminateSelfOnDisconnect, GetIOTaskRunner()),
        GetIOTaskRunner());
  }
#endif

  // Add filters passed here via options.
  if (options.with_legacy_ipc_channel) {
    DCHECK(legacy_ipc_bootstrap_pipe.is_valid());
    channel_->Init(IPC::ChannelFactory::CreateClientFactory(
                       std::move(legacy_ipc_bootstrap_pipe),
                       ChildProcess::current()->io_task_runner(),
                       ipc_task_runner_
                           ? ipc_task_runner_
                           : base::SingleThreadTaskRunner::GetCurrentDefault()),
                   /*create_pipe_now=*/true);
  }

  DCHECK(child_process_pipe_for_receiver.is_valid());

  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadState::BindChildProcessReceiver, io_thread_state_,
                     mojo::PendingReceiver<mojom::ChildProcess>(
                         std::move(child_process_pipe_for_receiver))));

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);
  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  if (!options.with_legacy_ipc_channel) {
    child_process_host_->Ping(
        base::BindOnce(&ChildThreadImpl::OnChannelConnected,
                       base::Unretained(this), /*unused=*/0));
  }
  main_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChildThreadImpl::EnsureConnected,
                     channel_connected_factory_->GetWeakPtr(),
                     connection_timeout),
      base::Seconds(connection_timeout));

  // In single-process mode, there is no need to synchronize trials to the
  // browser process (because it's the same process).
  if (!IsInBrowserProcess()) {
    mojo::PendingRemote<mojom::FieldTrialRecorder> pending_remote;
    BindHostReceiver(pending_remote.InitWithNewPipeAndPassReceiver());
    mojo::SharedRemote<mojom::FieldTrialRecorder> shared_remote(
        std::move(pending_remote));
    field_trial_syncer_ =
        variations::ChildProcessFieldTrialSyncer::CreateInstance(
            base::BindRepeating(&FieldTrialActivatedCallback,
                                std::move(shared_remote)));
  }
}

ChildThreadImpl::~ChildThreadImpl() {
  if (channel_) {
    // The ChannelProxy object caches a pointer to the IPC thread, so need to
    // reset it as it's not guaranteed to outlive this object.
    // NOTE: this also has the side-effect of not closing the main IPC channel
    // to the browser process.  This is needed because this is the signal that
    // the browser uses to know that this process has died, so we need it to be
    // alive until this process is shut down, and the OS closes the handle
    // automatically.  We used to watch the object handle on Windows to do this,
    // but it wasn't possible to do so on POSIX.
    channel_->ClearIPCTaskRunner();
  } else if (!IsInBrowserProcess()) {
    // With no legacy IPC channel, the browser monitors the lifetime of the
    // ChildProcessHost connection to detect our exit. For reasons similar to
    // above, we leak our side of this connection to ensure that the browser
    // does not observe disconnection until after our process is actually
    // terminated.
    auto leaked_remote =
        std::make_unique<mojo::SharedRemote<mojom::ChildProcessHost>>(
            std::move(child_process_host_));
    [[maybe_unused]] auto* leaked_remote_ptr = leaked_remote.release();
    ANNOTATE_LEAKING_OBJECT_PTR(leaked_remote_ptr);
  }
}

void ChildThreadImpl::Shutdown() {
  // Ensure that our IOThreadState's last ref goes away on the IO thread.
  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(io_thread_state_)));
}

bool ChildThreadImpl::ShouldBeDestroyed() {
  return true;
}

void ChildThreadImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_factory_.reset();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;
  // If this thread runs in the browser process, only Thread::Stop should
  // stop its message loop. Otherwise, QuitWhenIdle could race Thread::Stop.
  if (!IsInBrowserProcess())
    quit_closure_.Run();
}

#if BUILDFLAG(IS_WIN)
void ChildThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  GetFontCacheWin()->PreCacheFont(log_font);
}

void ChildThreadImpl::ReleaseCachedFonts() {
  GetFontCacheWin()->ReleaseCachedFonts();
}

const mojo::Remote<mojom::FontCacheWin>& ChildThreadImpl::GetFontCacheWin() {
  if (!font_cache_win_)
    BindHostReceiver(font_cache_win_.BindNewPipeAndPassReceiver());
  return font_cache_win_;
}
#endif

void ChildThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  NOTREACHED();
}

void ChildThreadImpl::RecordComputedAction(const std::string& action) {
  NOTREACHED();
}

void ChildThreadImpl::BindHostReceiver(mojo::GenericPendingReceiver receiver) {
  if (child_process_host_)
    child_process_host_->BindHostReceiver(std::move(receiver));
}

void ChildThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // All associated interfaces are requested through RenderThreadImpl.
  LOG(ERROR) << "Receiver for unknown Channel-associated interface: "
             << interface_name;
  DUMP_WILL_BE_NOTREACHED();
}

void ChildThreadImpl::ExposeInterfacesToBrowser(mojo::BinderMap binders) {
  // NOTE: Do not add new binders directly within this method. Instead, modify
  // the definition of |ExposeChildInterfacesToBrowser()|, ensuring security
  // review coverage.
  ExposeChildInterfacesToBrowser(GetIOTaskRunner(), IsInBrowserProcess(),
                                 &binders);

  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IOThreadState::ExposeInterfacesToBrowser,
                                io_thread_state_, std::move(binders)));
}

void ChildThreadImpl::GetBackgroundTracingAgentProvider(
    mojo::PendingReceiver<tracing::mojom::BackgroundTracingAgentProvider>
        receiver) {
  if (!background_tracing_agent_provider_) {
    background_tracing_agent_provider_ =
        std::make_unique<tracing::BackgroundTracingAgentProviderImpl>();
  }
  background_tracing_agent_provider_->AddBinding(std::move(receiver));
}

void ChildThreadImpl::DisconnectChildProcessHost() {
  child_process_host_.reset();
}

void ChildThreadImpl::BindServiceInterface(
    mojo::GenericPendingReceiver receiver) {
  DLOG(ERROR) << "Ignoring unhandled request to bind service interface: "
              << *receiver.interface_name();
}

void ChildThreadImpl::OnBindReceiver(mojo::GenericPendingReceiver receiver) {}

ChildThreadImpl* ChildThreadImpl::current() {
  return child_thread_impl;
}

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_)
    return;

  quit_closure_.Run();
}

void ChildThreadImpl::SetBatterySaverMode(bool battery_saver_mode_enabled) {}

void ChildThreadImpl::EnsureConnected(int connection_timeout) {
  VLOG(0) << "Terminating current process after " << connection_timeout
          << " seconds with no connection.";
  base::Process::TerminateCurrentProcessImmediately(0);
}

bool ChildThreadImpl::IsInBrowserProcess() const {
  return static_cast<bool>(browser_process_io_runner_);
}

#if BUILDFLAG(IS_ANDROID)
void ChildThreadImpl::OnMemoryPressureFromBrowserReceived(
    base::MemoryPressureLevel level) {
  // Generate no memory pressure signals when --single-process is specified.
  // Because we expect a signal for the browser process has been already
  // generated.
  if (IsInBrowserProcess()) {
    return;
  }
  // Forward the notification to the registry of MemoryPressureListeners.
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}
#endif

}  // namespace content
