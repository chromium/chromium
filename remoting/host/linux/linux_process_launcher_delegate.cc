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
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/host/base/switches.h"

namespace remoting {

namespace {

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

LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    base::CommandLine command_line)
    : command_line(std::move(command_line)) {}

LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    LaunchOptions&&) = default;
LinuxWorkerProcessLauncherDelegate::LaunchOptions::LaunchOptions(
    const LaunchOptions&) = default;
LinuxWorkerProcessLauncherDelegate::LaunchOptions::~LaunchOptions() = default;

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

  mojo::OutgoingInvitation::Send(std::move(invitation), process.Handle(),
                                 channel.TakeLocalEndpoint());

  channel_ = std::move(server);

  // TODO: crbug.com/475611769 - watch for process exits.
}

void LinuxWorkerProcessLauncherDelegate::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::CloseChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::CrashProcess(
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::KillProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::OnChannelError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void LinuxWorkerProcessLauncherDelegate::ReportFatalError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WorkerProcessLauncher* event_handler = event_handler_;
  event_handler_ = nullptr;
  event_handler->OnFatalError();
}

}  // namespace remoting
