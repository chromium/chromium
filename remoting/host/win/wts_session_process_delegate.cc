// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/wts_session_process_delegate.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/crash/crash_reporting_crashpad.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/win/io_completion_port_drainer.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/wts_terminal_monitor.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/host/worker_process_launcher.h"

// Name of the default session desktop.
const char kDefaultDesktopName[] = "winsta0\\default";

// Fallback poll interval while waiting for terminated job processes to finish
// kernel rundown in case `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` is dropped under
// kernel resource pressure.
constexpr base::TimeDelta kJobDrainPollInterval = base::Milliseconds(50);

// Slower fallback poll interval used once `kJobDrainPollLogThreshold` has been
// reached so stuck job processes do not keep polling at 20 Hz indefinitely.
constexpr base::TimeDelta kJobDrainSlowPollInterval = base::Seconds(5);

// Number of fallback poll ticks before logging a warning once and backing off
// if processes still remain active after `TerminateJobObject()`.
constexpr uint32_t kJobDrainPollLogThreshold = 100;

namespace remoting {

using base::win::ScopedHandle;

// A private class actually implementing the functionality provided by
// |WtsSessionProcessDelegate|. This class is ref-counted and implements
// asynchronous fire-and-forget shutdown.
//
// Most methods of this class run on the caller's thread (the thread that
// created the Core object). However, it also uses an I/O task runner (the
// `io_task_runner_` member) to receive and handle job object notifications
// (e.g., process creation and exit events). The class coordinates between
// these two threads.
class WtsSessionProcessDelegate::Core
    : public base::RefCountedThreadSafe<Core>,
      public base::MessagePumpForIO::IOHandler,
      public IPC::Listener {
 public:
  Core(base::WeakPtr<WtsSessionProcessDelegate> delegate,
       scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
       std::unique_ptr<base::CommandLine> target,
       bool launch_elevated,
       const std::string& channel_security);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // Initializes the object returning true on success.
  bool Initialize(uint32_t session_id);

  // Stops the object asynchronously.
  void Stop();

  // Mirrors WorkerProcessLauncher::Delegate.
  void LaunchProcess(WorkerProcessLauncher* event_handler);
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver);
  void CloseChannel();
  void CrashProcess(const base::Location& location);
  void KillProcess();

  bool AssignProcessToJobForTesting(base::ProcessHandle process);
  void SetCoreDeletedCallbackForTesting(base::OnceClosure callback);
  void SimulateJobNotificationForTesting(DWORD event, base::ProcessId pid);
  base::ProcessId GetWorkerProcessPidForTesting() const;
  void SetElevatedLauncherPidForTesting(base::ProcessId pid);
  void SetWorkerProcessPidForTesting(base::ProcessId pid);
  void OnProcessLaunchDetectedForTesting(base::ProcessId pid);
  void SetElevatedServerEndpointForTesting(
      mojo::PlatformChannelServerEndpoint endpoint);
  void SetFatalErrorCallbackForTesting(base::OnceClosure callback);
  void SetProcessLaunchedCallbackForTesting(base::OnceClosure callback);
  void SetCompletionPortDrainerForTesting(
      std::unique_ptr<IoCompletionPortDrainer> drainer);
  void SetActiveProcessCountOverrideForTesting(
      base::RepeatingCallback<DWORD()> callback);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  enum class JobDrainState {
    // Either `!launch_elevated_` or `InitializeJobCompleted()` has not yet run.
    kNotInitialized,
    // `InitializeJob()` failed to register the drainer or job object.
    kInitializationFailed,
    // `job_` is registered with the I/O completion port and open.
    kActive,
    // `Stop()` has terminated `job_`, waiting for `ActiveProcesses` to reach 0.
    kWaitingForProcessesToExit,
    // `job_` is closed and `completion_port_drainer_` is draining the port.
    kDrainingCompletionPort,
    // The sentinel packet has completed; no job packets remain in flight.
    kDrained,
  };

  ~Core() override;

  // base::MessagePumpForIO::IOHandler implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override;

  // IPC::Listener implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // The actual implementation of LaunchProcess()
  void DoLaunchProcess();

  // Queries the active process count for `job_` (or runs the test override).
  DWORD QueryActiveProcessCount() const;

  // Fallback timer callback invoked while waiting for `ActiveProcesses` to
  // reach zero.
  void OnActiveProcessPollTimer();

  // Closes `job_` once all processes in the job have exited in the kernel and
  // initiates completion-port draining.
  void MaybeDrainJobNotifications(bool from_poll_timer = false);

  // Enqueues a sentinel packet via `completion_port_drainer_` on
  // `io_task_runner_` so `Core` stays alive until all prior packets have been
  // dequeued.
  void StartCompletionPortDrain();

  // Invoked on `caller_task_runner_` once the completion port has been drained.
  void OnJobNotificationsDrained();

  // Registers `job` and `drainer` with the I/O completion port.
  void InitializeJob(ScopedHandle job,
                     std::unique_ptr<IoCompletionPortDrainer> drainer);

  // Notified that the job object initialization is complete.
  void InitializeJobCompleted(ScopedHandle job,
                              std::unique_ptr<IoCompletionPortDrainer> drainer);

  // Notified on `caller_task_runner_` when `InitializeJob()` fails on the I/O
  // thread.
  void OnInitializeJobFailed();

  // Called when the number of processes running in the job reaches zero.
  void OnActiveProcessZero();

  // Called when a process is launched in |job_|.
  void OnProcessLaunchDetected(base::ProcessId pid);

  void ReportFatalError();
  void ReportProcessLaunched(base::win::ScopedHandle worker_process);

  // The delegate object.
  base::WeakPtr<WtsSessionProcessDelegate> delegate_;

  // The task runner all public methods of this class should be called on.
  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // The task runner serving job object notifications.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  // Security descriptor (as SDDL) to be applied to |channel_|.
  const std::string channel_security_;

  raw_ptr<WorkerProcessLauncher> event_handler_ = nullptr;

  // The job object used to control the lifetime of child processes.
  base::win::ScopedHandle job_;

  // True if the worker process should be launched elevated.
  const bool launch_elevated_;

  // True if a launch attempt is pending.
  bool launch_pending_ = false;

  // The token to be used to launch a process in a different session.
  base::win::ScopedHandle session_token_;

  // Command line of the launched process.
  const std::unique_ptr<base::CommandLine> target_command_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  // If launching elevated, this holds the server handle after launch, until
  // the final process launches.
  mojo::PlatformChannelServerEndpoint elevated_server_endpoint_;

  // If launching elevated, this is the pid of the launcher process.
  std::atomic<base::ProcessId> elevated_launcher_pid_ = base::kNullProcessId;

  // Tracks the id of the worker process.
  std::atomic<base::ProcessId> worker_process_pid_ = base::kNullProcessId;

  // The pending process connection for the process being launched.
  mojo::OutgoingInvitation mojo_invitation_;

  mojo::AssociatedRemote<mojom::WorkerProcessControl> worker_process_control_;

  // Keeps this object alive while `job_` is open and registered with the
  // completion port. Transferred into the drain callback once `job_` is closed.
  scoped_refptr<Core> self_;

  // True if `Stop()` has been called.
  bool stopped_ = false;

  // Tracks job teardown and completion-port draining on `caller_task_runner_`.
  JobDrainState job_drain_state_ = JobDrainState::kNotInitialized;

  // Polls `job_` accounting as a fallback if `ACTIVE_PROCESS_ZERO` is dropped.
  base::OneShotTimer active_process_poll_timer_;
  uint32_t active_process_poll_count_ = 0;
  bool logged_active_process_poll_stall_ = false;

  // Drains `io_task_runner_`'s completion port after `job_` is closed.
  std::unique_ptr<IoCompletionPortDrainer> completion_port_drainer_;
  std::unique_ptr<IoCompletionPortDrainer> completion_port_drainer_for_testing_;
  base::RepeatingCallback<DWORD()> active_process_count_override_for_testing_;

  base::OnceClosure deleted_callback_for_testing_;
  base::OnceClosure fatal_error_callback_for_testing_;
  base::OnceClosure process_launched_callback_for_testing_;
};

WtsSessionProcessDelegate::Core::Core(
    base::WeakPtr<WtsSessionProcessDelegate> delegate,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    std::unique_ptr<base::CommandLine> target_command,
    bool launch_elevated,
    const std::string& channel_security)
    : base::MessagePumpForIO::IOHandler(FROM_HERE),
      delegate_(delegate),
      caller_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(std::move(io_task_runner)),
      channel_security_(channel_security),
      launch_elevated_(launch_elevated),
      target_command_(std::move(target_command)) {}

bool WtsSessionProcessDelegate::Core::Initialize(uint32_t session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (launch_elevated_) {
    // GetNamedPipeClientProcessId() is available starting from Vista.
    HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
    CHECK(kernel32 != nullptr);

    ScopedHandle job;
    job.Set(CreateJobObject(nullptr, nullptr));
    if (!job.is_valid()) {
      PLOG(ERROR) << "Failed to create a job object";
      return false;
    }

    // Limit the number of active processes in the job to two (the helper
    // process performing elevation and the worker process itself) and make sure
    // that all processes will be killed once the job object is destroyed.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    UNSAFE_TODO(memset(&info, 0, sizeof(info)));
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
                                            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                            JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    info.BasicLimitInformation.ActiveProcessLimit = 2;
    if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation,
                                 &info, sizeof(info))) {
      PLOG(ERROR) << "Failed to set limits on the job object";
      return false;
    }

    std::unique_ptr<IoCompletionPortDrainer> drainer =
        completion_port_drainer_for_testing_
            ? std::move(completion_port_drainer_for_testing_)
            : IoCompletionPortDrainer::Create();
    if (!drainer) {
      return false;
    }

    // To receive job object notifications the job object is registered with
    // the completion port represented by `io_task_runner_`. The registration
    // has to be done on the I/O thread because
    // `CurrentIOThread::RegisterJobObject()` can only be called on that thread.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::InitializeJob, this, std::move(job),
                                  std::move(drainer)));
  }

  // Create a session token for the launched process.
  return CreateSessionToken(session_id, &session_token_);
}

void WtsSessionProcessDelegate::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (stopped_) {
    return;
  }
  stopped_ = true;

  KillProcess();

  // Drain the completion queue to make sure all job object notifications have
  // been received.
  MaybeDrainJobNotifications();
}

void WtsSessionProcessDelegate::Core::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!event_handler_);

  event_handler_ = event_handler;
  if (delegate_) {
    delegate_->event_handler_ = event_handler;
  }
  DoLaunchProcess();
}

void WtsSessionProcessDelegate::Core::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  channel_->GetRemoteAssociatedInterface(std::move(receiver));
}

void WtsSessionProcessDelegate::Core::CloseChannel() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!channel_) {
    return;
  }

  worker_process_control_.reset();
  channel_.reset();
  elevated_server_endpoint_.reset();
  elevated_launcher_pid_ = base::kNullProcessId;
  worker_process_pid_ = base::kNullProcessId;
  mojo_invitation_ = {};
}

void WtsSessionProcessDelegate::Core::CrashProcess(
    const base::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (worker_process_control_) {
    worker_process_control_->CrashProcess(
        location.function_name(), location.file_name(), location.line_number());
  }
}

void WtsSessionProcessDelegate::Core::KillProcess() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  CloseChannel();

  event_handler_ = nullptr;
  launch_pending_ = false;

  if (launch_elevated_) {
    if (job_.is_valid()) {
      TerminateJobObject(job_.Get(), CONTROL_C_EXIT);
    }
  } else {
    if (worker_process_.is_valid()) {
      TerminateProcess(worker_process_.Get(), CONTROL_C_EXIT);
    }
  }

  worker_process_.Close();
}

bool WtsSessionProcessDelegate::Core::AssignProcessToJobForTesting(
    base::ProcessHandle process) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  if (stopped_ || !job_.is_valid() ||
      !::AssignProcessToJobObject(job_.Get(), process)) {
    return false;
  }
  return true;
}

void WtsSessionProcessDelegate::Core::SetCoreDeletedCallbackForTesting(
    base::OnceClosure callback) {
  deleted_callback_for_testing_ = std::move(callback);
}

void WtsSessionProcessDelegate::Core::SimulateJobNotificationForTesting(
    DWORD event,
    base::ProcessId pid) {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::SimulateJobNotificationForTesting,
                                  this, event, pid));
    return;
  }
  OnIOCompleted(reinterpret_cast<base::MessagePumpForIO::IOContext*>(
                    static_cast<uintptr_t>(pid)),
                event, 0);
}

base::ProcessId WtsSessionProcessDelegate::Core::GetWorkerProcessPidForTesting()
    const {
  return worker_process_pid_.load();
}

void WtsSessionProcessDelegate::Core::SetElevatedLauncherPidForTesting(
    base::ProcessId pid) {
  elevated_launcher_pid_ = pid;
}

void WtsSessionProcessDelegate::Core::SetWorkerProcessPidForTesting(
    base::ProcessId pid) {
  worker_process_pid_ = pid;
}

void WtsSessionProcessDelegate::Core::OnProcessLaunchDetectedForTesting(
    base::ProcessId pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  OnProcessLaunchDetected(pid);
}

void WtsSessionProcessDelegate::Core::SetElevatedServerEndpointForTesting(
    mojo::PlatformChannelServerEndpoint endpoint) {
  elevated_server_endpoint_ = std::move(endpoint);
}

void WtsSessionProcessDelegate::Core::SetFatalErrorCallbackForTesting(
    base::OnceClosure callback) {
  fatal_error_callback_for_testing_ = std::move(callback);
}

void WtsSessionProcessDelegate::Core::SetProcessLaunchedCallbackForTesting(
    base::OnceClosure callback) {
  process_launched_callback_for_testing_ = std::move(callback);
}

void WtsSessionProcessDelegate::Core::SetCompletionPortDrainerForTesting(
    std::unique_ptr<IoCompletionPortDrainer> drainer) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(job_drain_state_, JobDrainState::kNotInitialized);
  completion_port_drainer_for_testing_ = std::move(drainer);
}

void WtsSessionProcessDelegate::Core::SetActiveProcessCountOverrideForTesting(
    base::RepeatingCallback<DWORD()> callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  active_process_count_override_for_testing_ = std::move(callback);
}

WtsSessionProcessDelegate::Core::~Core() {
  DCHECK(!channel_);
  DCHECK(!event_handler_);
  DCHECK(!worker_process_.is_valid());
  DCHECK(!job_.is_valid());
  DCHECK(!self_);
  if (deleted_callback_for_testing_) {
    std::move(deleted_callback_for_testing_).Run();
  }
}

void WtsSessionProcessDelegate::Core::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // |bytes_transferred| is used in job object notifications to supply
  // the message ID; |context| carries process ID for the events we listen for.
  base::ProcessId process_id =
      static_cast<base::ProcessId>(reinterpret_cast<uintptr_t>(context));
  switch (bytes_transferred) {
    case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO: {
      caller_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&Core::OnActiveProcessZero, this));
      break;
    }
    case JOB_OBJECT_MSG_NEW_PROCESS: {
      if (elevated_launcher_pid_.load() == base::kNullProcessId) {
        // Ignore process launch events when we don't have a valid launcher pid.
        return;
      }

      if (process_id != elevated_launcher_pid_.load()) {
        DCHECK_EQ(worker_process_pid_.load(), base::kNullProcessId);
        worker_process_pid_ = process_id;
      }
      break;
    }
    case JOB_OBJECT_MSG_EXIT_PROCESS:
    case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS: {
      if (process_id == worker_process_pid_.load()) {
        // In official builds the first launch of a UiAccess enabled binary
        // will fail due to 'STATUS_ELEVATION_REQUIRED'.  This is an artifact of
        // using ShellExecuteEx() to launch the process.  In this scenario, we
        // will clear out the previously stored value for |worker_process_pid_|
        // and retry after the subsequent relaunch of the worker process.
        worker_process_pid_ = base::kNullProcessId;
      } else if (process_id == elevated_launcher_pid_.load()) {
        if (worker_process_pid_.load() == base::kNullProcessId) {
          // The elevated launcher process can fail to launch without attemping
          // to launch the worker.  In this scenario, the failure will be
          // detected outside this method and the elevated launcher will be
          // launched again.
          return;
        }

        caller_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&Core::OnProcessLaunchDetected, this,
                                      worker_process_pid_.load()));
      }
      break;
    }
  }
}

void WtsSessionProcessDelegate::Core::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  channel_->GetRemoteAssociatedInterface(&worker_process_control_);

  if (event_handler_) {
    event_handler_->OnChannelConnected(peer_pid);
  }
}

void WtsSessionProcessDelegate::Core::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  event_handler_->OnChannelError();
}

void WtsSessionProcessDelegate::Core::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  event_handler_->OnAssociatedInterfaceRequest(interface_name,
                                               std::move(handle));
}

void WtsSessionProcessDelegate::Core::DoLaunchProcess() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!channel_);
  DCHECK(!worker_process_.is_valid());

  if (stopped_) {
    return;
  }

  base::CommandLine command_line(target_command_->argv());
  if (launch_elevated_) {
    if (job_drain_state_ == JobDrainState::kInitializationFailed) {
      LOG(ERROR) << "Cannot launch elevated worker: job initialization failed.";
      ReportFatalError();
      return;
    }

    // The job object is not ready. Retry starting the host process later.
    if (!job_.is_valid()) {
      launch_pending_ = true;
      return;
    }

    // Construct the helper binary name.
    base::FilePath helper_binary;
    if (!GetInstalledBinaryPath(kHostBinaryName, &helper_binary)) {
      ReportFatalError();
      return;
    }

    // Create the command line passing the name of the IPC channel to use and
    // copying known switches from the caller's command line.
    command_line.SetProgram(helper_binary);
    command_line.AppendSwitchPath(kElevateSwitchName,
                                  target_command_->GetProgram());
  }

  std::string mojo_pipe_token = base::NumberToString(base::RandUint64());
  channel_ = IPC::ChannelProxy::Create(
      mojo_invitation_.AttachMessagePipe(mojo_pipe_token),
      IPC::Channel::MODE_SERVER, this, io_task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  command_line.AppendSwitchASCII(kMojoPipeToken, mojo_pipe_token);

  std::wstring crashpad_pipe = GetCrashpadHandlerPipe();
  if (!crashpad_pipe.empty()) {
    command_line.AppendSwitchNative(kCrashpadHandlerPipe, crashpad_pipe);
  }

  std::unique_ptr<mojo::PlatformChannel> normal_mojo_channel;
  std::unique_ptr<mojo::NamedPlatformChannel> elevated_mojo_channel;
  base::HandlesToInheritVector handles_to_inherit;
  if (launch_elevated_) {
    // Pass the name of the IPC channel to use.
    mojo::NamedPlatformChannel::Options options;
    options.security_descriptor = base::UTF8ToWide(channel_security_);
    elevated_mojo_channel =
        std::make_unique<mojo::NamedPlatformChannel>(options);
    elevated_mojo_channel->PassServerNameOnCommandLine(&command_line);
  } else {
    normal_mojo_channel = std::make_unique<mojo::PlatformChannel>();
    normal_mojo_channel->PrepareToPassRemoteEndpoint(&handles_to_inherit,
                                                     &command_line);
  }

  // Try to launch the process.
  ScopedHandle worker_process;
  ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(
          command_line.GetProgram(), command_line.GetCommandLineString(),
          session_token_.Get(), /*security_attributes=*/nullptr,
          /*thread_attributes=*/nullptr, handles_to_inherit,
          /*security_capabilities=*/nullptr,
          /*creation_flags=*/CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
          base::UTF8ToWide(kDefaultDesktopName).c_str(), &worker_process,
          &worker_thread)) {
    ReportFatalError();
    return;
  }

  if (launch_elevated_) {
    elevated_launcher_pid_ = GetProcessId(worker_process.Get());
    if (!AssignProcessToJobObject(job_.Get(), worker_process.Get())) {
      PLOG(ERROR) << "Failed to assign the worker to the job object";
      ReportFatalError();
      return;
    }
  }

  if (!ResumeThread(worker_thread.Get())) {
    PLOG(ERROR) << "Failed to resume the worker thread";
    ReportFatalError();
    return;
  }

  if (launch_elevated_) {
    // When launching an elevated worker process, an intermediate launcher
    // process launches the worker process. Reporting the launch waits until the
    // worker process launch is detected. Until then, store the values needed in
    // fields. See OnProcessLaunchDetected for their use.
    elevated_server_endpoint_ = elevated_mojo_channel->TakeServerEndpoint();
    DCHECK(elevated_server_endpoint_.is_valid());
  } else {
    mojo::OutgoingInvitation::Send(std::move(mojo_invitation_),
                                   worker_process.Get(),
                                   normal_mojo_channel->TakeLocalEndpoint());
    ReportProcessLaunched(std::move(worker_process));
  }
}

DWORD WtsSessionProcessDelegate::Core::QueryActiveProcessCount() const {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(job_.is_valid());

  if (active_process_count_override_for_testing_) {
    return active_process_count_override_for_testing_.Run();
  }

  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
  PCHECK(::QueryInformationJobObject(job_.Get(),
                                     JobObjectBasicAccountingInformation, &info,
                                     sizeof(info), nullptr));
  return info.ActiveProcesses;
}

void WtsSessionProcessDelegate::Core::OnActiveProcessPollTimer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(stopped_);
  MaybeDrainJobNotifications(/*from_poll_timer=*/true);
}

void WtsSessionProcessDelegate::Core::MaybeDrainJobNotifications(
    bool from_poll_timer) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(stopped_);

  if (job_drain_state_ != JobDrainState::kActive &&
      job_drain_state_ != JobDrainState::kWaitingForProcessesToExit) {
    return;
  }

  if (QueryActiveProcessCount() > 0) {
    // Closing the last handle to the job destroys the kernel job object only
    // once every process in the job has terminated. `TerminateJobObject()` does
    // not wait for processes to finish kernel rundown, so keep `job_` open
    // until `ActiveProcesses` reaches zero.
    job_drain_state_ = JobDrainState::kWaitingForProcessesToExit;
    if (from_poll_timer) {
      ::TerminateJobObject(job_.Get(), CONTROL_C_EXIT);
      if (!logged_active_process_poll_stall_ &&
          ++active_process_poll_count_ >= kJobDrainPollLogThreshold) {
        logged_active_process_poll_stall_ = true;
        LOG(WARNING) << "Job object still has active processes after "
                     << (kJobDrainPollInterval * active_process_poll_count_)
                     << "; continuing to retry TerminateJobObject().";
      }
    }
    if (!active_process_poll_timer_.IsRunning()) {
      const base::TimeDelta poll_interval = logged_active_process_poll_stall_
                                                ? kJobDrainSlowPollInterval
                                                : kJobDrainPollInterval;
      active_process_poll_timer_.Start(
          FROM_HERE, poll_interval,
          base::BindOnce(&Core::OnActiveProcessPollTimer,
                         base::Unretained(this)));
    }
    return;
  }

  active_process_poll_timer_.Stop();
  job_drain_state_ = JobDrainState::kDrainingCompletionPort;

  // Assume that once `QueryInformationJobObject()` reports `ActiveProcesses` as
  // zero, Windows has already queued the final job notifications for those
  // processes to the completion port (this relies on NT kernel behavior rather
  // than a documented API guarantee; a violation would leave a packet behind
  // the sentinel and result in a use-after-free). Closing `job_` destroys the
  // kernel job object so no further job notifications can be generated.
  job_.Close();

  // Enqueue a real completion packet onto the I/O completion port via
  // `completion_port_drainer_` rather than relying on a regular posted task:
  // `MessagePumpForIO` interleaves application tasks with individual completion
  // packets, so a posted task can overtake job packets still queued behind
  // other handlers' packets.
  DCHECK(self_);
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::StartCompletionPortDrain, std::move(self_)));
}

void WtsSessionProcessDelegate::Core::StartCompletionPortDrain() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  CHECK(completion_port_drainer_);

  completion_port_drainer_->Drain(base::BindPostTask(
      caller_task_runner_,
      base::BindOnce(&Core::OnJobNotificationsDrained, this)));
}

void WtsSessionProcessDelegate::Core::OnJobNotificationsDrained() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(job_drain_state_, JobDrainState::kDrainingCompletionPort);

  job_drain_state_ = JobDrainState::kDrained;
  completion_port_drainer_.reset();
}

void WtsSessionProcessDelegate::Core::InitializeJob(
    ScopedHandle job,
    std::unique_ptr<IoCompletionPortDrainer> drainer) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!drainer->RegisterOnIoThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::OnInitializeJobFailed, this));
    return;
  }

  // Register to receive job notifications via the I/O thread's completion port.
  if (!base::CurrentIOThread::Get()->RegisterJobObject(job.Get(), this)) {
    PLOG(ERROR) << "Failed to associate the job object with a completion port";
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::OnInitializeJobFailed, this));
    return;
  }

  // Let the main thread know that initialization is complete.
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::InitializeJobCompleted, this,
                                std::move(job), std::move(drainer)));
}

void WtsSessionProcessDelegate::Core::InitializeJobCompleted(
    ScopedHandle job,
    std::unique_ptr<IoCompletionPortDrainer> drainer) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!job_.is_valid());
  DCHECK_EQ(job_drain_state_, JobDrainState::kNotInitialized);

  job_ = std::move(job);
  completion_port_drainer_ = std::move(drainer);

  // Keep this object alive until the job object notifications have been
  // drained. This ensures that `OnIOCompleted()` is not called with a dangling
  // pointer.
  self_ = this;
  job_drain_state_ = JobDrainState::kActive;

  if (stopped_) {
    // If `Stop()` was called before the job was initialized, we must still
    // ensure the completion port is drained because `InitializeJob()` has
    // already called `RegisterJobObject()`.
    MaybeDrainJobNotifications();
    return;
  }

  if (launch_pending_) {
    DoLaunchProcess();
  }
}

void WtsSessionProcessDelegate::Core::OnInitializeJobFailed() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!job_.is_valid());
  DCHECK_EQ(job_drain_state_, JobDrainState::kNotInitialized);

  job_drain_state_ = JobDrainState::kInitializationFailed;
  if (stopped_) {
    return;
  }
  if (launch_pending_) {
    launch_pending_ = false;
    ReportFatalError();
  }
}

void WtsSessionProcessDelegate::Core::OnActiveProcessZero() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (stopped_) {
    MaybeDrainJobNotifications();
    return;
  }

  if (job_.is_valid()) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
    if (QueryInformationJobObject(job_.Get(),
                                  JobObjectBasicAccountingInformation, &info,
                                  sizeof(info), nullptr) &&
        info.ActiveProcesses > 0) {
      return;
    }
  }

  if (launch_pending_) {
    LOG(ERROR) << "The worker process exited before connecting via IPC.";
    launch_pending_ = false;
    ReportFatalError();
  }
}

void WtsSessionProcessDelegate::Core::OnProcessLaunchDetected(
    base::ProcessId pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(pid, elevated_launcher_pid_.load());

  if (!elevated_server_endpoint_.is_valid()) {
    return;
  }

  DWORD desired_access =
      SYNCHRONIZE | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION;
  base::win::ScopedHandle worker_process(
      OpenProcess(desired_access, false, pid));
  if (!worker_process.is_valid()) {
    PLOG(ERROR) << "Failed to open process " << pid;
    ReportFatalError();
    return;
  }

  BOOL is_in_job = FALSE;
  if (!job_.is_valid() ||
      !::IsProcessInJob(worker_process.Get(), job_.Get(), &is_in_job) ||
      !is_in_job) {
    LOG(ERROR) << "Process " << pid << " is not in the expected job object.";
    ReportFatalError();
    return;
  }

  elevated_launcher_pid_ = base::kNullProcessId;
  mojo::OutgoingInvitation::Send(std::move(mojo_invitation_),
                                 worker_process.Get(),
                                 std::move(elevated_server_endpoint_));
  ReportProcessLaunched(std::move(worker_process));
}

void WtsSessionProcessDelegate::Core::ReportFatalError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  CloseChannel();

  WorkerProcessLauncher* event_handler = event_handler_;
  event_handler_ = nullptr;
  if (event_handler) {
    event_handler->OnFatalError();
  }
  if (fatal_error_callback_for_testing_) {
    std::move(fatal_error_callback_for_testing_).Run();
  }
}

void WtsSessionProcessDelegate::Core::ReportProcessLaunched(
    base::win::ScopedHandle worker_process) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!worker_process_.is_valid());

  worker_process_ = std::move(worker_process);

  // Report a handle that can be used to wait for the worker process completion,
  // query information about the process and duplicate handles.
  DWORD desired_access =
      SYNCHRONIZE | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION;
  HANDLE temp_handle;
  if (!DuplicateHandle(GetCurrentProcess(), worker_process_.Get(),
                       GetCurrentProcess(), &temp_handle, desired_access, FALSE,
                       0)) {
    PLOG(ERROR) << "Failed to duplicate a handle";
    ReportFatalError();
    return;
  }
  ScopedHandle limited_handle(temp_handle);

  if (delegate_ && event_handler_) {
    delegate_->WatchProcess(std::move(limited_handle));
  }
  if (process_launched_callback_for_testing_) {
    std::move(process_launched_callback_for_testing_).Run();
  }
}

WtsSessionProcessDelegate::WtsSessionProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    std::unique_ptr<base::CommandLine> target_command,
    bool launch_elevated,
    const std::string& channel_security) {
  core_ = base::MakeRefCounted<Core>(weak_ptr_factory_.GetWeakPtr(),
                                     io_task_runner, std::move(target_command),
                                     launch_elevated, channel_security);
}

WtsSessionProcessDelegate::~WtsSessionProcessDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

bool WtsSessionProcessDelegate::Initialize(uint32_t session_id) {
  return core_->Initialize(session_id);
}

void WtsSessionProcessDelegate::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  core_->LaunchProcess(event_handler);
}

void WtsSessionProcessDelegate::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  core_->GetRemoteAssociatedInterface(std::move(receiver));
}

void WtsSessionProcessDelegate::CloseChannel() {
  core_->CloseChannel();
}

void WtsSessionProcessDelegate::CrashProcess(const base::Location& location) {
  core_->CrashProcess(location);
}

void WtsSessionProcessDelegate::KillProcess() {
  core_->KillProcess();
}

WtsSessionProcessDelegate::TestApi::TestApi(WtsSessionProcessDelegate* delegate)
    : delegate_(delegate) {}

bool WtsSessionProcessDelegate::TestApi::AssignProcessToJob(
    base::ProcessHandle process) {
  return delegate_->core_->AssignProcessToJobForTesting(process);
}

void WtsSessionProcessDelegate::TestApi::SetCoreDeletedCallback(
    base::OnceClosure callback) {
  delegate_->core_->SetCoreDeletedCallbackForTesting(std::move(callback));
}

void WtsSessionProcessDelegate::TestApi::SimulateJobNotification(
    DWORD event,
    base::ProcessId pid) {
  delegate_->core_->SimulateJobNotificationForTesting(event, pid);
}

base::ProcessId WtsSessionProcessDelegate::TestApi::GetWorkerProcessPid()
    const {
  return delegate_->core_->GetWorkerProcessPidForTesting();
}

void WtsSessionProcessDelegate::TestApi::SetElevatedLauncherPid(
    base::ProcessId pid) {
  delegate_->core_->SetElevatedLauncherPidForTesting(pid);
}

void WtsSessionProcessDelegate::TestApi::SetWorkerProcessPid(
    base::ProcessId pid) {
  delegate_->core_->SetWorkerProcessPidForTesting(pid);
}

void WtsSessionProcessDelegate::TestApi::OnProcessLaunchDetected(
    base::ProcessId pid) {
  delegate_->core_->OnProcessLaunchDetectedForTesting(pid);
}

void WtsSessionProcessDelegate::TestApi::SetElevatedServerEndpoint(
    mojo::PlatformChannelServerEndpoint endpoint) {
  delegate_->core_->SetElevatedServerEndpointForTesting(std::move(endpoint));
}

void WtsSessionProcessDelegate::TestApi::SetFatalErrorCallback(
    base::OnceClosure callback) {
  delegate_->core_->SetFatalErrorCallbackForTesting(std::move(callback));
}

void WtsSessionProcessDelegate::TestApi::SetProcessLaunchedCallback(
    base::OnceClosure callback) {
  delegate_->core_->SetProcessLaunchedCallbackForTesting(std::move(callback));
}

void WtsSessionProcessDelegate::TestApi::SetCompletionPortDrainer(
    std::unique_ptr<IoCompletionPortDrainer> drainer) {
  delegate_->core_->SetCompletionPortDrainerForTesting(  // IN-TEST
      std::move(drainer));
}

void WtsSessionProcessDelegate::TestApi::SetActiveProcessCountOverride(
    base::RepeatingCallback<DWORD()> callback) {
  delegate_->core_->SetActiveProcessCountOverrideForTesting(  // IN-TEST
      std::move(callback));
}

base::MessagePumpForIO::IOHandler*
WtsSessionProcessDelegate::TestApi::GetJobIoHandler() {
  return delegate_->core_.get();
}

}  // namespace remoting
