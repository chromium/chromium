// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/linux_process_launcher_delegate.h"

#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"

namespace remoting {

namespace {

// An interval to wait for process exit. This allows
// LinuxWorkerProcessLauncherDelegate to stop and destroy ProcessExitWatcher
// when the watcher is watching. Otherwise the watcher thread will be blocked
// indefinitely.
constexpr base::TimeDelta kWaitForExitInterval = base::Seconds(10);

class RunAsUserPreExecDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  RunAsUserPreExecDelegate(bool new_session, int uid, int gid)
      : new_session_(new_session), uid_(uid), gid_(gid) {}
  ~RunAsUserPreExecDelegate() override = default;

  RunAsUserPreExecDelegate(const RunAsUserPreExecDelegate&) = delete;
  RunAsUserPreExecDelegate& operator=(const RunAsUserPreExecDelegate&) = delete;

  void RunAsyncSafe() override {
    if (new_session_) {
      pid_t new_sid = setsid();
      if (new_sid == -1) {
        RAW_LOG(FATAL, "Failed to create a new session.");
      }
    }
    if (gid_ >= 0 && setgid(gid_) != 0) {
      RAW_LOG(FATAL, "Failed to setgid");
    }
    if (uid_ >= 0 && setuid(uid_) != 0) {
      RAW_LOG(FATAL, "Failed to setuid");
    }
    // Kill the child process when the parent is dead.
    // base::LaunchOptions::kill_on_parent_death does the same thing, but it is
    // called before setsid(), which will get reset.
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) {
      RAW_LOG(FATAL, "prctl(PR_SET_PDEATHSIG) failed");
    }
  }

 private:
  bool new_session_;
  int uid_;
  int gid_;
};

}  // namespace

class LinuxWorkerProcessLauncherDelegate::ProcessExitWatcher {
 public:
  using OnProcessExitedCallback = base::OnceCallback<void(int /*exit_code*/)>;

  ProcessExitWatcher(base::Process process,
                     OnProcessExitedCallback on_process_exited);
  ~ProcessExitWatcher();

  ProcessExitWatcher(const ProcessExitWatcher&) = delete;
  ProcessExitWatcher& operator=(const ProcessExitWatcher&) = delete;

 private:
  void WaitForExit();
  void PostTaskToWaitForExit();

  SEQUENCE_CHECKER(sequence_checker_);

  base::Process process_ GUARDED_BY_CONTEXT(sequence_checker_);
  OnProcessExitedCallback on_process_exited_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<ProcessExitWatcher> weak_ptr_factory_{this};
};

LinuxWorkerProcessLauncherDelegate::ProcessExitWatcher::ProcessExitWatcher(
    base::Process process,
    OnProcessExitedCallback on_process_exited)
    : process_(std::move(process)),
      on_process_exited_(std::move(on_process_exited)) {
  PostTaskToWaitForExit();
}

LinuxWorkerProcessLauncherDelegate::ProcessExitWatcher::~ProcessExitWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LinuxWorkerProcessLauncherDelegate::ProcessExitWatcher::WaitForExit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int exit_code = 0;
  bool exited =
      process_.WaitForExitWithTimeout(kWaitForExitInterval, &exit_code);
  if (exited) {
    std::move(on_process_exited_).Run(exit_code);
  } else {
    PostTaskToWaitForExit();
  }
}

void LinuxWorkerProcessLauncherDelegate::ProcessExitWatcher::
    PostTaskToWaitForExit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessExitWatcher::WaitForExit,
                                weak_ptr_factory_.GetWeakPtr()));
}

// LinuxWorkerProcessLauncherDelegate::LaunchOptions:

LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    base::CommandLine command_line)
    : command_line(std::move(command_line)) {}

LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    LaunchOptions&&) = default;
LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    const LaunchOptions&) = default;
LinuxWorkerProcessLauncherDelegate::LaunchOptions::~LaunchOptions() = default;

// LinuxWorkerProcessLauncherDelegate:

LinuxWorkerProcessLauncherDelegate::LinuxWorkerProcessLauncherDelegate(
    LaunchOptions options,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : options_(std::move(options)), io_task_runner_(io_task_runner) {}

LinuxWorkerProcessLauncherDelegate::~LinuxWorkerProcessLauncherDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LinuxWorkerProcessLauncherDelegate::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!event_handler_);
  event_handler_ = event_handler;

  base::LaunchOptions launch_options;
  if (!options_.working_dir.empty()) {
    launch_options.current_directory = std::move(options_.working_dir);
  }
  launch_options.clear_environment = true;
  launch_options.environment = std::move(options_.environment_variables);

  RunAsUserPreExecDelegate pre_exec_delegate(options_.new_session, options_.uid,
                                             options_.gid);
  launch_options.pre_exec_delegate = &pre_exec_delegate;

  mojo::OutgoingInvitation invitation;
  std::string message_pipe_token = base::NumberToString(base::RandUint64());
  std::unique_ptr<IPC::ChannelProxy> server = IPC::ChannelProxy::Create(
      invitation.AttachMessagePipe(message_pipe_token).release(),
      IPC::Channel::MODE_SERVER, this, io_task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  base::CommandLine command_line = options_.command_line;
  command_line.AppendSwitchASCII(kMojoPipeToken, message_pipe_token);
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&launch_options, &command_line);

  base::Process process = base::LaunchProcess(command_line, launch_options);

  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch process.";
    ReportFatalError();
    return;
  }
  worker_process_ = std::move(process);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 worker_process_.Handle(),
                                 channel.TakeLocalEndpoint());

  channel_ = std::move(server);

  // TODO: crbug.com/475611769 - watch for process exits.
}

void LinuxWorkerProcessLauncherDelegate::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  channel_->GetRemoteAssociatedInterface(std::move(receiver));
}

void LinuxWorkerProcessLauncherDelegate::CloseChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  worker_process_control_.reset();
  channel_.reset();
}

void LinuxWorkerProcessLauncherDelegate::CrashProcess(
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (worker_process_control_) {
    worker_process_control_->CrashProcess(
        location.function_name(), location.file_name(), location.line_number());
  }
}

void LinuxWorkerProcessLauncherDelegate::KillProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CloseChannel();
  event_handler_ = nullptr;

  if (worker_process_.IsValid()) {
    // Note that the exit code is not used on Linux.
    worker_process_.Terminate(kSuccessExitCode, /*wait=*/true);
    worker_process_.Close();
  }
}

void LinuxWorkerProcessLauncherDelegate::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_process_.IsValid());

  if (peer_pid != worker_process_.Pid()) {
    LOG(ERROR) << "The actual client PID " << worker_process_.Pid()
               << " does not match the one reported by the client: "
               << peer_pid;
    ReportFatalError();
    return;
  }

  channel_->GetRemoteAssociatedInterface(&worker_process_control_);

  event_handler_->OnChannelConnected(peer_pid);
}

void LinuxWorkerProcessLauncherDelegate::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  event_handler_->OnChannelError();
}

void LinuxWorkerProcessLauncherDelegate::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  event_handler_->OnAssociatedInterfaceRequest(interface_name,
                                               std::move(handle));
}

void LinuxWorkerProcessLauncherDelegate::WatchForProcessExit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_process_.IsValid());
  DCHECK(process_exit_watcher_.is_null());

  auto task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  process_exit_watcher_.emplace(
      task_runner, worker_process_.Duplicate(),
      base::BindOnce(&LinuxWorkerProcessLauncherDelegate::OnProcessExited,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LinuxWorkerProcessLauncherDelegate::OnProcessExited(int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  worker_process_.Close();
  process_exit_watcher_.Reset();

  event_handler_->OnProcessExited(exit_code);
}

void LinuxWorkerProcessLauncherDelegate::ReportFatalError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CloseChannel();

  WorkerProcessLauncher* event_handler = event_handler_;
  event_handler_ = nullptr;
  event_handler->OnFatalError();
}

}  // namespace remoting
