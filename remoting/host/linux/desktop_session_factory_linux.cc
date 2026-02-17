// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_session_factory_linux.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/linux/linux_process_launcher_delegate.h"
#include "remoting/host/linux/remote_display_session_manager.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/pam_utils.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/host/worker_process_launcher.h"

namespace remoting {

namespace {

std::string IdToDisplayName(int id) {
  return base::NumberToString(id);
}

}  // namespace

class DesktopSessionFactoryLinux::DesktopSessionLinux
    : public DesktopSession,
      public WorkerProcessIpcDelegate,
      public mojom::DesktopSessionRequestHandler {
 public:
  DesktopSessionLinux(
      DaemonProcess* daemon_process,
      int id,
      std::string_view display_name,
      std::string_view required_username,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::OnceClosure remove_from_factory);
  ~DesktopSessionLinux() override;

  void OnRemoteDisplaySessionChanged(
      const RemoteDisplaySessionManager::RemoteDisplayInfo& info);

  // Notifies the daemon process and terminates the desktop session. Note that
  // `this` will be deleted during the call.
  void TerminateSession();

  // DesktopSession implementation.
  void SetScreenResolution(const ScreenResolution& resolution) override;

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnPermanentError(int exit_code) override;
  void OnWorkerProcessStopped() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::DesktopSessionRequestHandler implementation.
  void ConnectDesktopChannel(
      mojo::ScopedMessagePipeHandle desktop_pipe) override;
  void InjectSecureAttentionSequence() override;
  void CrashNetworkProcess() override;

  base::WeakPtr<DesktopSessionLinux> GetWeakPtr();

 private:
  void CrashDesktopProcess(const base::Location& location);

  // Returns whether the current desktop session is allowed based on
  // `required_username_`. If the session info is not ready yet, this method
  // will still return true, since it will be called again once the session info
  // is ready.
  bool IsSessionUsernameAllowed(
      const RemoteDisplaySessionManager::RemoteDisplayInfo& info);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string display_name_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string required_username_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure remove_from_factory_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<WorkerProcessLauncher> launcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::AssociatedReceiver<mojom::DesktopSessionRequestHandler>
      desktop_session_request_handler_ GUARDED_BY_CONTEXT(sequence_checker_){
          this};

  base::WeakPtrFactory<DesktopSessionLinux> weak_ptr_factory_{this};
};

DesktopSessionFactoryLinux::DesktopSessionLinux::DesktopSessionLinux(
    DaemonProcess* daemon_process,
    int id,
    std::string_view display_name,
    std::string_view required_username,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::OnceClosure remove_from_factory)
    : DesktopSession(daemon_process, id),
      display_name_(display_name),
      required_username_(required_username),
      io_task_runner_(io_task_runner),
      remove_from_factory_(std::move(remove_from_factory)) {}

DesktopSessionFactoryLinux::DesktopSessionLinux::~DesktopSessionLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(remove_from_factory_).Run();
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::
    OnRemoteDisplaySessionChanged(
        const RemoteDisplaySessionManager::RemoteDisplayInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!info.session_info.has_value() || !info.user_info.has_value() ||
      info.environment_variables.empty()) {
    // Session is not ready yet, or is detached. Kill the desktop process if
    // it is running.
    launcher_.reset();
    return;
  }

  if (!IsSessionUsernameAllowed(info)) {
    LOG(ERROR) << "User " << info.user_info->username
               << " is not allowed for local login.";
    // TODO: crbug.com/475611769 - Pass the SESSION_REJECTED error code to the
    // network process so that the client can see the correct error message.
    TerminateSession();
    return;
  }

  if (!IsLocalLoginAllowed(info.user_info->username)) {
    LOG(ERROR) << "User " << info.user_info->username
               << " is not allowed for local login.";
    // TODO: crbug.com/475611769 - Pass the SESSION_REJECTED error code to the
    // network process so that the client can see the correct error message.
    TerminateSession();
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
  options.uid = info.user_info->uid;
  options.gid = info.user_info->gid;
  options.working_dir = info.user_info->home_dir;
  options.environment_variables = info.environment_variables;

  // Launch the desktop process. If there is a desktop process running for the
  // previous desktop session, this will kill it.
  launcher_ = std::make_unique<WorkerProcessLauncher>(
      std::make_unique<LinuxWorkerProcessLauncherDelegate>(std::move(options),
                                                           io_task_runner_),
      this);
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::TerminateSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The daemon process will delete `this`.
  daemon_process()->CloseDesktopSession(id());
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::SetScreenResolution(
    const ScreenResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO: crbug.com/475611769 - Implement.
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::OnChannelConnected(
    int32_t peer_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "IPC: daemon <- desktop (" << peer_pid << ")";
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::OnPermanentError(
    int exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TerminateSession();
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::OnWorkerProcessStopped() {
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::
    OnAssociatedInterfaceRequest(const std::string& interface_name,
                                 mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (interface_name == mojom::DesktopSessionRequestHandler::Name_) {
    if (desktop_session_request_handler_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionRequestHandler::Name_;
      CrashDesktopProcess(FROM_HERE);
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionRequestHandler>
        pending_receiver(std::move(handle));
    desktop_session_request_handler_.Bind(std::move(pending_receiver));

    // Reset the receiver on disconnect so |desktop_session_request_handler_|
    // can be re-bound if |launcher_| spawns a new desktop process.
    desktop_session_request_handler_.reset_on_disconnect();
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing the desktop process";
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::ConnectDesktopChannel(
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!daemon_process()->OnDesktopSessionAgentAttached(
          id(), /*session_id=*/0, std::move(desktop_pipe))) {
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::
    InjectSecureAttentionSequence() {
  NOTIMPLEMENTED();
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::CrashNetworkProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  daemon_process()->CrashNetworkProcess(FROM_HERE);
}

base::WeakPtr<DesktopSessionFactoryLinux::DesktopSessionLinux>
DesktopSessionFactoryLinux::DesktopSessionLinux::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DesktopSessionFactoryLinux::DesktopSessionLinux::CrashDesktopProcess(
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  launcher_->Crash(location);
}

bool DesktopSessionFactoryLinux::DesktopSessionLinux::IsSessionUsernameAllowed(
    const RemoteDisplaySessionManager::RemoteDisplayInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (required_username_.empty()) {
    return true;
  }
  if (!info.user_info.has_value() || !info.session_info.has_value()) {
    // The session info is not ready yet. This method will be called again when
    // it is ready, so we just return true here.
    return true;
  }
  if (info.session_info->session_class == "greeter") {
    HOST_LOG << "Login username check skipped for greeter session.";
    return true;
  }
  if (base::EqualsCaseInsensitiveASCII(required_username_,
                                       info.user_info->username)) {
    return true;
  }
  LOG(ERROR) << "User " << info.user_info->username
             << " does not match the required username: " << required_username_;
  return false;
}

// DesktopSessionFactoryLinux implementation.

DesktopSessionFactoryLinux::DesktopSessionFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner) {}

DesktopSessionFactoryLinux::~DesktopSessionFactoryLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DesktopSessionFactoryLinux::Start(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_display_session_manager_.Start(
      this,
      base::BindOnce(&DesktopSessionFactoryLinux::OnStartResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<DesktopSession>
DesktopSessionFactoryLinux::CreateDesktopSession(
    int id,
    DaemonProcess* daemon_process,
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name = IdToDisplayName(id);
  if (desktop_sessions_.contains(display_name)) {
    LOG(ERROR) << "Desktop session ID " << id << " is already in use.";
    return nullptr;
  }
  auto desktop_session = std::make_unique<DesktopSessionLinux>(
      daemon_process, id, display_name, options.required_username,
      io_task_runner_,
      base::BindOnce(&DesktopSessionFactoryLinux::RemoveDesktopSession,
                     weak_ptr_factory_.GetWeakPtr(), display_name));
  // TODO: crbug.com/475611769 - Add timeout mechanism for waiting for the
  // desktop session.
  remote_display_session_manager_.CreateRemoteDisplay(
      display_name,
      base::BindOnce(&DesktopSessionFactoryLinux::OnCreateRemoteDisplayResult,
                     weak_ptr_factory_.GetWeakPtr(), display_name));
  desktop_sessions_[display_name] = desktop_session->GetWeakPtr();
  return desktop_session;
}

void DesktopSessionFactoryLinux::OnStartResult(
    Callback callback,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    // If there are any pre-existing remote displays with the CRD prefix, then
    // they are probably leaked from the previous CRD host incarnation. We can't
    // reuse them so we just terminate them to prevent collisions of the remote
    // display names.
    // TODO: crbug.com/475611769 - see how we can recover these sessions. We may
    // need to write something to the disk.
    for (const auto& [display_name, _] :
         remote_display_session_manager_.remote_displays()) {
      HOST_LOG << "Terminating pre-existing remote display: " << display_name;
      remote_display_session_manager_.TerminateRemoteDisplay(
          display_name,
          base::BindOnce([](base::expected<void, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << result.error();
            }
          }));
    }
  }
  std::move(callback).Run(std::move(result));
}

void DesktopSessionFactoryLinux::OnCreateRemoteDisplayResult(
    std::string_view display_name,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    // No need to do anything. OnRemoteDisplaySessionChanged() will be called
    // once the session is ready.
    return;
  }

  LOG(ERROR) << result.error();
  auto session = FindSession(display_name);
  // session may be nullptr if the DesktopSession has been destroyed.
  if (session) {
    session->TerminateSession();
  }
}

void DesktopSessionFactoryLinux::OnRemoteDisplaySessionChanged(
    std::string_view display_name,
    const RemoteDisplaySessionManager::RemoteDisplayInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto session = FindSession(display_name);
  if (session) {
    session->OnRemoteDisplaySessionChanged(info);
  }
}

void DesktopSessionFactoryLinux::OnRemoteDisplayTerminated(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto session = FindSession(display_name);
  // session may be nullptr if the desktop session has already been removed by
  // RemoveDesktopSession().
  if (session) {
    // TODO: crbug.com/475611769 - Pass the SESSION_REJECTED error code to the
    // network process so that the client can see the correct error message.
    session->TerminateSession();
  }
}

void DesktopSessionFactoryLinux::RemoveDesktopSession(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  desktop_sessions_.erase(display_name);
  if (!remote_display_session_manager_.GetRemoteDisplayInfo(display_name)) {
    // The remote display has already been terminated.
    return;
  }
  remote_display_session_manager_.TerminateRemoteDisplay(
      display_name, base::BindOnce([](base::expected<void, Loggable> result) {
        // TODO: crbug.com/475611769 - See what to do with the callback.
        if (!result.has_value()) {
          LOG(ERROR) << result.error();
        }
      }));
}

base::WeakPtr<DesktopSessionFactoryLinux::DesktopSessionLinux>
DesktopSessionFactoryLinux::FindSession(std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = desktop_sessions_.find(display_name);
  return it == desktop_sessions_.end() ? nullptr : it->second;
}

}  // namespace remoting
