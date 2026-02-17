// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include <optional>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_switch.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/active_field_trials.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/sandboxed_process_launcher_delegate.h"
#include "content/public/browser/tracing_support.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/configuration.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "services/tracing/public/cpp/trace_startup.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/launcher_thread.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "base/apple/mach_port_rendezvous_ios.h"
#include "base/files/scoped_temp_dir.h"
#endif

namespace content {
namespace internal {

namespace {

void RecordHistogramsOnLauncherThread(base::TimeDelta launch_time) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Log the launch time, separating out the first one (which will likely be
  // slower due to the rest of the browser initializing at the same time).
  static bool done_first_launch = false;
  if (done_first_launch) {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchSubsequent", launch_time);
  } else {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchFirst", launch_time);
    done_first_launch = true;
  }
}

// On POSIX, the descriptor is transferred to `files_to_register, which will
// then pass it to the zygote via the launch service.
void TransferSharedMemorySwitchDescriptor(
    base::shared_memory::SharedMemorySwitch& shared_memory_switch,
    FileMappedForLaunch* files_to_register) {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  CHECK(files_to_register);
  if (shared_memory_switch.out_descriptor_to_share.is_valid()) {
    files_to_register->Transfer(
        shared_memory_switch.descriptor_key,
        std::move(shared_memory_switch.out_descriptor_to_share));
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
}

// If the histogram shared memory region is valid and passing the histogram
// shared memory region via the command line is enabled, update the launch
// parameters to pass the shared memory handle. The allocation of the shared
// memory region is dependent on the process-type being launched, and non-fatal
// if not enabled.
void PassHistogramSharedMemoryHandle(
    const base::UnsafeSharedMemoryRegion* histogram_memory_region,
    base::CommandLine& command_line,
    base::LaunchOptions* launch_options,
    [[maybe_unused]] FileMappedForLaunch* files_to_register) {
  // TODO(crbug.com/40109064): Once all process types support histogram shared
  // memory being passed at launch, remove this if.
  const bool enabled =
      histogram_memory_region && histogram_memory_region->IsValid();
  DVLOG(1) << (enabled ? "A" : "Not a")
           << "dding histogram shared memory launch parameters for "
           << command_line.GetSwitchValueASCII(::switches::kProcessType)
           << " process.";
  if (!enabled) {
    return;
  }

  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      ::switches::kMetricsSharedMemoryHandle, 'hsmr',
      kHistogramSharedMemoryDescriptor);
  shared_memory_switch.AddToLaunchParameters(*histogram_memory_region,
                                             &command_line, launch_options);

  TransferSharedMemorySwitchDescriptor(shared_memory_switch, files_to_register);
}

// Update the process launch parameters to transmit the field trial shared
// memory handle to the child process via the command line.
void PassFieldTrialSharedMemoryHandle(
    base::CommandLine& command_line,
    base::LaunchOptions* launch_options,
    [[maybe_unused]] FileMappedForLaunch* files_to_register) {
  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      switches::kFieldTrialHandle, 'fldt', kFieldTrialDescriptor);
  variations::PopulateLaunchOptionsWithVariationsInfo(
      &shared_memory_switch, &command_line, launch_options);
  TransferSharedMemorySwitchDescriptor(shared_memory_switch, files_to_register);
}

void PassStartupTracingConfigSharedMemoryHandle(
    const base::ReadOnlySharedMemoryRegion* read_only_memory_region,
    base::CommandLine& command_line,
    base::LaunchOptions* launch_options,
    [[maybe_unused]] FileMappedForLaunch* files_to_register) {
  if (!read_only_memory_region || !read_only_memory_region->IsValid()) {
    return;
  }

  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      switches::kTraceConfigHandle, 'trcc', kTraceConfigSharedMemoryDescriptor);
  shared_memory_switch.AddToLaunchParameters(*read_only_memory_region,
                                             &command_line, launch_options);
  TransferSharedMemorySwitchDescriptor(shared_memory_switch, files_to_register);
}

void PassStartupTracingOutputSharedMemoryHandle(
    const base::UnsafeSharedMemoryRegion* trace_output_memory_region,
    base::CommandLine& command_line,
    base::LaunchOptions* launch_options,
    [[maybe_unused]] FileMappedForLaunch* files_to_register) {
  if (!trace_output_memory_region || !trace_output_memory_region->IsValid()) {
    return;
  }

  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      switches::kTraceBufferHandle, 'trbc', kTraceOutputSharedMemoryDescriptor);
  shared_memory_switch.AddToLaunchParameters(*trace_output_memory_region,
                                             &command_line, launch_options);
  TransferSharedMemorySwitchDescriptor(shared_memory_switch, files_to_register);
}

// Passes the pseudonymization salt to child processes via shared memory,
// ensuring it's available before any Mojo IPCs. See https://crbug.com/40850085.
void PassPseudonymizationSaltSharedMemoryHandle(
    base::CommandLine& command_line,
    base::LaunchOptions* launch_options,
    [[maybe_unused]] FileMappedForLaunch* files_to_register) {
  // Salt must be initialized in PreCreateThreads() before any child process
  // launches. See BrowserMainLoop::PreCreateThreads().
  CHECK(IsSaltInitialized());

  const base::ReadOnlySharedMemoryRegion& salt_region =
      GetPseudonymizationSaltSharedMemoryRegion();
  CHECK(salt_region.IsValid());

  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      switches::kPseudonymizationSaltHandle, 'salt',
      kPseudonymizationSaltDescriptor);
  shared_memory_switch.AddToLaunchParameters(salt_region, &command_line,
                                             launch_options);
  TransferSharedMemorySwitchDescriptor(shared_memory_switch, files_to_register);
}

}  // namespace

ChildProcessLauncherHelper::Process::Process() = default;

ChildProcessLauncherHelper::Process::~Process() = default;

ChildProcessLauncherHelper::Process::Process(Process&& other)
    : process(std::move(other.process))
#if BUILDFLAG(USE_ZYGOTE)
      ,
      zygote(other.zygote)
#endif
#if BUILDFLAG(IS_FUCHSIA)
      ,
      sandbox_policy(std::move(other.sandbox_policy))
#endif
{
}

ChildProcessLauncherHelper::Process&
ChildProcessLauncherHelper::Process::Process::operator=(
    ChildProcessLauncherHelper::Process&& other) = default;

ChildProcessLauncherHelper::ChildProcessLauncherHelper(
    ChildProcessId child_process_id,
    std::unique_ptr<base::CommandLine> command_line,
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
    bool terminate_on_shutdown,
#if BUILDFLAG(IS_ANDROID)
    bool can_use_warm_up_connection,
    bool is_spare_renderer,
#endif
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback,
    std::unique_ptr<ChildProcessLauncherFileData> file_data,
    scoped_refptr<base::RefCountedData<base::UnsafeSharedMemoryRegion>>
        histogram_memory_region,
    scoped_refptr<base::RefCountedData<base::ReadOnlySharedMemoryRegion>>
        tracing_config_memory_region,
    scoped_refptr<base::RefCountedData<base::UnsafeSharedMemoryRegion>>
        tracing_output_memory_region)
    : child_process_id_(child_process_id),
      client_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      command_line_(std::move(command_line)),
      delegate_(std::move(delegate)),
      child_process_launcher_(child_process_launcher),
      terminate_on_shutdown_(terminate_on_shutdown),
      mojo_invitation_(std::move(mojo_invitation)),
      process_error_callback_(process_error_callback),
      file_data_(std::move(file_data)),
#if BUILDFLAG(IS_ANDROID)
      can_use_warm_up_connection_(can_use_warm_up_connection),
      is_spare_renderer_(is_spare_renderer),
#endif
      histogram_memory_region_(std::move(histogram_memory_region)),
      tracing_config_memory_region_(std::move(tracing_config_memory_region)),
      tracing_output_memory_region_(std::move(tracing_output_memory_region)),
      init_start_time_(base::TimeTicks::Now()) {
  if (!mojo::core::GetConfiguration().is_broker_process &&
      !command_line_->HasSwitch(switches::kDisableMojoBroker)) {
    command_line_->AppendSwitch(switches::kDisableMojoBroker);
  }
  // command_line_ is always accessed from the launcher thread, so detach it
  // from the client thread here.
  command_line_->DetachFromCurrentSequence();
}

ChildProcessLauncherHelper::~ChildProcessLauncherHelper() {
#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kSchedQoSOnResourcedForChrome) &&
      process_id_.has_value()) {
    base::Process::Open(process_id_.value()).ForgetPriority();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_IOS)
  GetProcessLauncherTaskRunner()->DeleteSoon(FROM_HERE,
                                             std::move(scoped_temp_dir_));
#endif
}

void ChildProcessLauncherHelper::StartLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  BeforeLaunchOnClientThread();

  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::LaunchOnLauncherThread,
                     this));
}

void ChildProcessLauncherHelper::LaunchOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  // Record the delay in getting to the launcher thread.
  UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLauncher.PreLaunchDelay",
                      base::TimeTicks::Now() - init_start_time_);

#if BUILDFLAG(IS_FUCHSIA)
  mojo_channel_.emplace();
#else   // BUILDFLAG(IS_FUCHSIA)
  mojo_named_channel_ = CreateNamedPlatformChannelOnLauncherThread();
  if (!mojo_named_channel_) {
    mojo_channel_.emplace();
  }
#endif  //  BUILDFLAG(IS_FUCHSIA)

  begin_launch_time_ = base::TimeTicks::Now();
  if (GetProcessType() == switches::kRendererProcess &&
      base::TimeTicks::IsConsistentAcrossProcesses()) {
    const base::TimeDelta ticks_as_delta = begin_launch_time_.since_origin();
    command_line()->AppendSwitchASCII(
        switches::kRendererProcessLaunchTimeTicks,
        base::NumberToString(ticks_as_delta.InMicroseconds()));
  }

  std::unique_ptr<FileMappedForLaunch> files_to_register = GetFilesToMap();

  bool is_synchronous_launch = true;
  int launch_result = LAUNCH_RESULT_FAILURE;
  std::optional<base::LaunchOptions> options;
  base::LaunchOptions* options_ptr = nullptr;
  if (IsUsingLaunchOptions()) {
    options.emplace();
    options_ptr = &*options;
#if BUILDFLAG(IS_WIN)
    options_ptr->elevated = delegate_->ShouldLaunchElevated();
#endif
  }

  // Propagate the kWaitForDebugger switch to child process if the
  // kWaitForDebuggerChildren is specified and matches the child process type.
  const base::CommandLine& current_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (current_command_line.HasSwitch(switches::kWaitForDebuggerChildren)) {
    std::string value = current_command_line.GetSwitchValueASCII(
        switches::kWaitForDebuggerChildren);
    if (value.empty() || value == GetProcessType()) {
      command_line()->AppendSwitch(switches::kWaitForDebugger);
    }
  }

  // Update the command line and launch options to pass the histogram and
  // field trial shared memory region handles.
  PassHistogramSharedMemoryHandle(
      histogram_memory_region_ ? &histogram_memory_region_->data : nullptr,
      *command_line(), options_ptr, files_to_register.get());
  PassFieldTrialSharedMemoryHandle(*command_line(), options_ptr,
                                   files_to_register.get());
  PassStartupTracingConfigSharedMemoryHandle(
      tracing_config_memory_region_ ? &tracing_config_memory_region_->data
                                    : nullptr,
      *command_line(), options_ptr, files_to_register.get());
  PassStartupTracingOutputSharedMemoryHandle(
      tracing_output_memory_region_ ? &tracing_output_memory_region_->data
                                    : nullptr,
      *command_line(), options_ptr, files_to_register.get());
  PassPseudonymizationSaltSharedMemoryHandle(*command_line(), options_ptr,
                                             files_to_register.get());

  auto track = GetChildProcessTracingTrack(child_process_id());
  command_line_->AppendSwitchASCII(switches::kTraceProcessTrackUuid,
                                   base::NumberToString(track.uuid));

  // Transfer logging switches & handles if necessary.
  PassLoggingSwitches(options_ptr, command_line());

  // Launch the child process.
  Process process;
  if (BeforeLaunchOnLauncherThread(*files_to_register, options_ptr)) {
    process = LaunchProcessOnLauncherThread(
        options_ptr, std::move(files_to_register),
#if BUILDFLAG(IS_ANDROID)
        can_use_warm_up_connection_, is_spare_renderer_,
#endif
        &is_synchronous_launch, &launch_result);
    AfterLaunchOnLauncherThread(process, options_ptr);
  }

  if (is_synchronous_launch) {
    // The LastError is set on the launcher thread, but needs to be transferred
    // to the Client thread.
    PostLaunchOnLauncherThread(std::move(process),
#if BUILDFLAG(IS_WIN)
                               ::GetLastError(),
#endif
                               launch_result);
  }
}

void ChildProcessLauncherHelper::PostLaunchOnLauncherThread(
    ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
    DWORD last_error,
#endif
    int launch_result) {
#if BUILDFLAG(IS_WIN)
  const bool launch_elevated = delegate_->ShouldLaunchElevated();
#else
  const bool launch_elevated = false;
#endif
  if (mojo_channel_)
    mojo_channel_->RemoteProcessLaunchAttempted();

  if (process.process.IsValid()) {
    RecordHistogramsOnLauncherThread(base::TimeTicks::Now() -
                                     begin_launch_time_);
  }

  // Take ownership of the broker client invitation here so it's destroyed when
  // we go out of scope regardless of the outcome below.
  mojo::OutgoingInvitation invitation = std::move(mojo_invitation_);
  if (launch_elevated) {
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
  }

#if BUILDFLAG(IS_WIN)
  if (delegate_->ShouldUseUntrustedMojoInvitation()) {
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS);
  }
#endif

  if (!mojo::core::GetConfiguration().is_broker_process) {
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
  }

  if (process.process.IsValid()) {
#if !BUILDFLAG(IS_FUCHSIA)
    if (mojo_named_channel_) {
      DCHECK(!mojo_channel_);
      mojo::OutgoingInvitation::Send(
          std::move(invitation), base::kNullProcessHandle,
          mojo_named_channel_->TakeServerEndpoint(), process_error_callback_);
    } else
#endif
    // Set up Mojo IPC to the new process.
    {
      DCHECK(mojo_channel_);
      DCHECK(mojo_channel_->local_endpoint().is_valid());
      mojo::OutgoingInvitation::Send(
          std::move(invitation), process.process.Handle(),
          mojo_channel_->TakeLocalEndpoint(), process_error_callback_);
    }
  }

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::PostLaunchOnClientThread,
                     this, std::move(process),
#if BUILDFLAG(IS_WIN)
                     last_error,
#endif
                     launch_result));
}

void ChildProcessLauncherHelper::PostLaunchOnClientThread(
    ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
    DWORD last_error,
#endif
    int error_code) {
  if (child_process_launcher_) {
    // Record the total launch duration.
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLauncher.Notify",
                        base::TimeTicks::Now() - init_start_time_);

    child_process_launcher_->Notify(std::move(process),
#if BUILDFLAG(IS_WIN)
                                    last_error,
#endif
                                    error_code);
  } else if (process.process.IsValid() && terminate_on_shutdown_) {
    // Client is gone, terminate the process.
    ForceNormalProcessTerminationAsync(std::move(process));
  }
}

std::string ChildProcessLauncherHelper::GetProcessType() {
  return command_line()->GetSwitchValueASCII(switches::kProcessType);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
    ChildProcessLauncherHelper::Process process) {
  if (CurrentlyOnProcessLauncherTaskRunner()) {
    ForceNormalProcessTerminationSync(std::move(process));
    return;
  }
  // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!
  // So don't do this on the UI/IO threads.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::ForceNormalProcessTerminationSync,
          std::move(process)));
}

#if !BUILDFLAG(IS_WIN)
void ChildProcessLauncherHelper::PassLoggingSwitches(
    base::LaunchOptions* launch_options,
    base::CommandLine* cmd_line) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  constexpr const char* kForwardSwitches[] = {
      switches::kDisableLogging,
      switches::kEnableLogging,
      switches::kLogFile,
      switches::kLoggingLevel,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches);
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace internal

// static
base::SingleThreadTaskRunner* GetProcessLauncherTaskRunner() {
#if BUILDFLAG(IS_ANDROID)
  // Android specializes Launcher thread so it is accessible in java.
  // Note Android never does clean shutdown, so shutdown use-after-free
  // concerns are not a problem in practice.
  // This process launcher thread will use the Java-side process-launching
  // thread, instead of creating its own separate thread on C++ side. Note
  // that means this thread will not be joined on shutdown, and may cause
  // use-after-free if anything tries to access objects deleted by
  // AtExitManager, such as non-leaky LazyInstance.
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      launcher_task_runner(android::LauncherThread::GetTaskRunner());
  return (*launcher_task_runner).get();
#else   // BUILDFLAG(IS_ANDROID)
  // TODO(http://crbug.com/820200): Investigate whether we could use
  // SequencedTaskRunner on platforms other than Windows.
  static base::LazyThreadPoolSingleThreadTaskRunner launcher_task_runner =
      LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
          base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                           base::TaskShutdownBehavior::BLOCK_SHUTDOWN),
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  return launcher_task_runner.Get().get();
#endif  // BUILDFLAG(IS_ANDROID)
}

// static
bool CurrentlyOnProcessLauncherTaskRunner() {
  return GetProcessLauncherTaskRunner()->RunsTasksInCurrentSequence();
}

}  // namespace content
