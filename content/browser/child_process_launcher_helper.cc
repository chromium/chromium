// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/launcher_thread.h"
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
    int child_process_id,
    std::unique_ptr<base::CommandLine> command_line,
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
    bool terminate_on_shutdown,
#if BUILDFLAG(IS_ANDROID)
    bool can_use_warm_up_connection,
#endif
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback,
    std::unique_ptr<ChildProcessLauncherFileData> file_data)
    : child_process_id_(child_process_id),
      client_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      command_line_(std::move(command_line)),
      delegate_(std::move(delegate)),
      child_process_launcher_(child_process_launcher),
      terminate_on_shutdown_(terminate_on_shutdown),
      mojo_invitation_(std::move(mojo_invitation)),
      process_error_callback_(process_error_callback),
      file_data_(std::move(file_data))
#if BUILDFLAG(IS_ANDROID)
      ,
      can_use_warm_up_connection_(can_use_warm_up_connection)
#endif
{
}

ChildProcessLauncherHelper::~ChildProcessLauncherHelper() = default;

void ChildProcessLauncherHelper::StartLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  BeforeLaunchOnClientThread();

#if BUILDFLAG(IS_FUCHSIA)
  mojo_channel_.emplace();
#else   // BUILDFLAG(IS_FUCHSIA)
  mojo_named_channel_ = CreateNamedPlatformChannelOnClientThread();
  if (!mojo_named_channel_)
    mojo_channel_.emplace();
#endif  //  BUILDFLAG(IS_FUCHSIA)

  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::LaunchOnLauncherThread,
                     this));
}

void ChildProcessLauncherHelper::LaunchOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  begin_launch_time_ = base::TimeTicks::Now();
  if (GetProcessType() == switches::kRendererProcess &&
      base::TimeTicks::IsConsistentAcrossProcesses()) {
    const base::TimeDelta ticks_as_delta = begin_launch_time_.since_origin();
    command_line_->AppendSwitchASCII(
        switches::kRendererProcessLaunchTimeTicks,
        base::NumberToString(ticks_as_delta.InMicroseconds()));
  }

  std::unique_ptr<FileMappedForLaunch> files_to_register = GetFilesToMap();

  bool is_synchronous_launch = true;
  int launch_result = LAUNCH_RESULT_FAILURE;
  absl::optional<base::LaunchOptions> options;
  base::LaunchOptions* options_ptr = nullptr;
  if (IsUsingLaunchOptions()) {
    options.emplace();
    options_ptr = &*options;
  }

  Process process;
  if (BeforeLaunchOnLauncherThread(*files_to_register, options_ptr)) {
    base::FieldTrialList::PopulateLaunchOptionsWithFieldTrialState(
        command_line(), options_ptr);
    process =
        LaunchProcessOnLauncherThread(options_ptr, std::move(files_to_register),
#if BUILDFLAG(IS_ANDROID)
                                      can_use_warm_up_connection_,
#endif
                                      &is_synchronous_launch, &launch_result);
    AfterLaunchOnLauncherThread(process, options_ptr);
  }

  if (is_synchronous_launch) {
    PostLaunchOnLauncherThread(std::move(process), launch_result);
  }
}

void ChildProcessLauncherHelper::PostLaunchOnLauncherThread(
    ChildProcessLauncherHelper::Process process,
    int launch_result) {
#if BUILDFLAG(IS_WIN)
  // The LastError is set on the launcher thread, but needs to be transferred to
  // the Client thread.
  DWORD last_error = ::GetLastError();
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
