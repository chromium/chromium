// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_session_linux.h"

#include <sys/types.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/source_location.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/linux/linux_process_launcher_delegate.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/pam_utils.h"
#include "remoting/host/worker_process_launcher.h"

namespace remoting {

namespace {

// Returns the path to `cgroup.procs` for the given logind session. Under
// systemd's cgroup hierarchy specification (`systemd.slice(5)` and
// https://systemd.io/CONTROL_GROUP_INTERFACE/), a session scope unit
// `session-<id>.scope` inside the user slice `user-<uid>.slice` maps to
// `/sys/fs/cgroup/user.slice/user-<uid>.slice/session-<id>.scope/cgroup.procs`.
base::FilePath GetSessionCgroupProcsPath(uid_t uid,
                                         std::string_view session_id) {
  // Check cgroup v2 path.
  base::FilePath cgroup_v2_path(base::StringPrintf(
      "/sys/fs/cgroup/user.slice/user-%u.slice/session-%s.scope/cgroup.procs",
      uid, std::string(session_id).c_str()));
  if (base::PathExists(cgroup_v2_path)) {
    return cgroup_v2_path;
  }
  // Check cgroup v1 systemd hierarchy fallback.
  return base::FilePath(base::StringPrintf(
      "/sys/fs/cgroup/systemd/user.slice/user-%u.slice/session-%s.scope/"
      "cgroup.procs",
      uid, std::string(session_id).c_str()));
}

}  // namespace

DesktopSessionLinux::DesktopSessionLinux(
    DaemonProcess* daemon_process,
    int id,
    std::string_view required_username,
    std::string_view client_id,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    DestroyedCallback on_destroyed,
    bool is_greeter_allowed)
    : DesktopSession(daemon_process, id),
      required_username_(required_username),
      client_id_(client_id),
      io_task_runner_(io_task_runner),
      on_destroyed_(std::move(on_destroyed)),
      is_greeter_allowed_(is_greeter_allowed) {}

DesktopSessionLinux::~DesktopSessionLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_destroyed_) {
    std::move(on_destroyed_).Run();
  }
}

void DesktopSessionLinux::SetSessionInfo(
    const LoginSessionManager::SessionInfo& session_info,
    const PasswdUserInfo& user_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_session_id_ = session_info.session_id;
  current_uid_ = user_info.uid;

  if (session_info.uid != user_info.uid) {
    TerminateSession(
        ErrorCode::SESSION_REJECTED,
        base::StringPrintf("Session UID (%u) does not match user UID (%u).",
                           session_info.uid, user_info.uid),
        FROM_HERE);
    return;
  }

  if (!IsSessionUsernameAllowed(session_info, user_info)) {
    TerminateSession(
        ErrorCode::SESSION_REJECTED,
        base::StringPrintf("User %s does not match the required username.",
                           user_info.username.c_str()),
        FROM_HERE);
    return;
  }

  // Root (UID 0) is unconditionally forbidden. System accounts (UID < 1000 or
  // nobody: 65534) are not allowed for user sessions.
  if (user_info.uid == 0 ||
      (session_info.session_class != "greeter" &&
       (user_info.uid < 1000 || user_info.uid == 65534))) {
    TerminateSession(
        ErrorCode::SESSION_REJECTED,
        base::StringPrintf(
            "User %s (UID %u) is not allowed for desktop sessions.",
            user_info.username.c_str(), user_info.uid),
        FROM_HERE);
    return;
  }

  if (!IsLocalLoginAllowed(user_info.username)) {
    TerminateSession(
        ErrorCode::SESSION_REJECTED,
        base::StringPrintf("Local login for user %s is disallowed by PAM.",
                           user_info.username.c_str()),
        FROM_HERE);
    return;
  }

  // TODO: crbug.com/475611769 - See if we need a dedicated desktop process
  // binary.
  base::FilePath this_exe;
  if (!base::PathService::Get(base::BasePathKey::FILE_EXE, &this_exe)) {
    LOG(ERROR) << "Failed to get the current executable path.";
    TerminateSession();
    return;
  }

  base::CommandLine command_line(this_exe);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeDesktop);

  LinuxWorkerProcessLauncherDelegate::LaunchOptions options(command_line);
  options.new_session = true;
  options.uid = user_info.uid;
  options.gid = user_info.gid;
  options.supplementary_gids = user_info.supplementary_gids;
  options.working_dir = user_info.home_dir;

  // Reset Mojo endpoints before destroying/reassigning `launcher_`.
  // WorkerProcessLauncher does not synchronously invoke
  // OnWorkerProcessStopped() upon destruction. Resetting here prevents a race
  // condition where the newly launched worker process connects before the old
  // channel's asynchronous disconnect notification runs, which would cause
  // OnAssociatedInterfaceRequest() to see an already-bound receiver and
  // mistakenly crash the new worker process.
  desktop_session_request_handler_.reset();
  desktop_process_control_.reset();

  // Launch the desktop process. If there is a desktop process running for the
  // previous desktop session, this will kill it.
  launcher_ = std::make_unique<WorkerProcessLauncher>(
      std::make_unique<LinuxWorkerProcessLauncherDelegate>(std::move(options),
                                                           io_task_runner_),
      this);
}

void DesktopSessionLinux::ClearSessionInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  launcher_.reset();
  desktop_session_request_handler_.reset();
  desktop_process_control_.reset();
  current_session_id_.clear();
  current_uid_.reset();
}

void DesktopSessionLinux::TerminateSession(
    ErrorCode error_code,
    const std::string& error_details,
    const SourceLocation& error_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!error_details.empty() || error_code != ErrorCode::OK) {
    LOG(ERROR) << "Terminating session " << id()
               << " (error code: " << ErrorCodeToString(error_code)
               << "): " << error_details << " at " << error_location.ToString();
  }

  // The daemon process will delete `this`.
  daemon_process()->CloseDesktopSessionWithError(id(), error_code,
                                                 error_details, error_location);
}

void DesktopSessionLinux::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No-op since screen resolution change is always handled by the desktop
  // process.
}

void DesktopSessionLinux::ReconnectNetworkChannel(
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (options.required_username != required_username_) {
    TerminateSession(ErrorCode::SESSION_REJECTED,
                     "Required username has changed.", FROM_HERE);
    return;
  }

  if (options.client_id != client_id_) {
    TerminateSession(ErrorCode::SESSION_REJECTED, "Client ID has changed.",
                     FROM_HERE);
    return;
  }

  if (desktop_process_control_.is_bound()) {
    desktop_process_control_->ReconnectNetworkChannel();
  }
  // If `desktop_process_control_` is not bound, then it means the desktop
  // process isn't launched yet. It will send the desktop pipe after it is
  // launched anyway so we don't need to do anything.
}

void DesktopSessionLinux::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "IPC: daemon <- desktop (" << peer_pid << ")";

  AttachProcessToSession(peer_pid);

  desktop_process_control_.reset();
  launcher_->GetRemoteAssociatedInterface(
      desktop_process_control_.BindNewEndpointAndPassReceiver());
}

void DesktopSessionLinux::OnPermanentError(int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TerminateSession();
}

void DesktopSessionLinux::OnWorkerProcessStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  desktop_process_control_.reset();
  desktop_session_request_handler_.reset();
}

void DesktopSessionLinux::AttachProcessToSession(int32_t pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pid <= 0) {
    LOG(WARNING) << "Cannot attach invalid process PID " << pid
                 << " to logind session.";
    return;
  }
  if (current_session_id_.empty() || !current_uid_.has_value()) {
    LOG(WARNING) << "Cannot attach process " << pid
                 << " to logind session: session info is not set.";
    return;
  }

  base::FilePath cgroup_procs_path =
      GetSessionCgroupProcsPath(*current_uid_, current_session_id_);
  if (!base::PathExists(cgroup_procs_path)) {
    LOG(WARNING) << "cgroup.procs path does not exist: " << cgroup_procs_path;
    return;
  }

  HOST_LOG << "Attaching desktop process " << pid << " to session cgroup "
           << cgroup_procs_path;

  std::string pid_str = base::NumberToString(pid);
  if (!base::WriteFile(cgroup_procs_path, pid_str)) {
    PLOG(WARNING) << "Failed to write PID " << pid << " to "
                  << cgroup_procs_path;
    return;
  }
  HOST_LOG << "Successfully attached desktop process " << pid
           << " to session cgroup " << cgroup_procs_path;
}

void DesktopSessionLinux::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (interface_name == mojom::DesktopSessionRequestHandler::Name_) {
    if (desktop_session_request_handler_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionRequestHandler::Name_;
      CrashDesktopProcess(FROM_HERE);
      return;
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionRequestHandler>
        pending_receiver(std::move(handle));
    desktop_session_request_handler_.Bind(std::move(pending_receiver));

    // Reset the receiver on disconnect so `desktop_session_request_handler_`
    // can be re-bound if `launcher_` spawns a new desktop process.
    desktop_session_request_handler_.reset_on_disconnect();
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing the desktop process";
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionLinux::ConnectDesktopChannel(
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!daemon_process()->OnDesktopSessionAgentAttached(
          id(), std::move(desktop_pipe))) {
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionLinux::InjectSecureAttentionSequence() {
  NOTIMPLEMENTED();
}

void DesktopSessionLinux::CrashNetworkProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  daemon_process()->CrashNetworkProcess(FROM_HERE);
}

base::WeakPtr<DesktopSessionLinux> DesktopSessionLinux::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DesktopSessionLinux::CrashDesktopProcess(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  launcher_->Crash(location);
}

bool DesktopSessionLinux::IsSessionUsernameAllowed(
    const LoginSessionManager::SessionInfo& session_info,
    const PasswdUserInfo& user_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_info.session_class == "greeter") {
    if (is_greeter_allowed_) {
      HOST_LOG << "Login username check skipped for greeter session.";
      return true;
    }
    LOG(ERROR) << "Greeter session is not allowed for this desktop session.";
    return false;
  }

  if (required_username_.empty()) {
    return true;
  }

  if (base::EqualsCaseInsensitiveASCII(required_username_,
                                       user_info.username)) {
    return true;
  }
  LOG(ERROR) << "User " << user_info.username
             << " does not match the required username: " << required_username_;
  return false;
}

}  // namespace remoting
