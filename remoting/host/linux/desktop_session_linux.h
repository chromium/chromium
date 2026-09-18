// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_SESSION_LINUX_H_
#define REMOTING_HOST_LINUX_DESKTOP_SESSION_LINUX_H_

#include <sys/types.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/base/errors.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/source_location.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace remoting {

class DaemonProcess;
class WorkerProcessLauncher;

// Manages the desktop worker process (`remoting_me2me_host --type=desktop`)
// running setuid as the target user, attaches the worker process to the
// session's cgroup, and handles IPC between daemon and desktop processes.
class DesktopSessionLinux : public DesktopSession,
                            public WorkerProcessIpcDelegate,
                            public mojom::DesktopSessionRequestHandler {
 public:
  using DestroyedCallback = base::OnceClosure;

  DesktopSessionLinux(
      DaemonProcess* daemon_process,
      int id,
      std::string_view required_username,
      std::string_view client_id,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      DestroyedCallback on_destroyed,
      bool is_greeter_allowed = false);
  ~DesktopSessionLinux() override;

  DesktopSessionLinux(const DesktopSessionLinux&) = delete;
  DesktopSessionLinux& operator=(const DesktopSessionLinux&) = delete;

  // Associates this session with a systemd login session and user credentials,
  // launching or re-launching the desktop process.
  void SetSessionInfo(const LoginSessionManager::SessionInfo& session_info,
                      const PasswdUserInfo& user_info);

  // Clears active session info and stops the desktop process if running.
  void ClearSessionInfo();

  // Terminates the session and notifies DaemonProcess.
  void TerminateSession(ErrorCode error_code = ErrorCode::OK,
                        const std::string& error_details = {},
                        const SourceLocation& error_location = FROM_HERE);

  const std::string& client_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return client_id_;
  }
  const std::string& current_session_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return current_session_id_;
  }
  std::optional<uid_t> current_uid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return current_uid_;
  }

  // DesktopSession implementation.
  void SetScreenResolution(const ScreenResolution& resolution) override;
  void ReconnectNetworkChannel(
      const mojom::DesktopSessionOptions& options) override;

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
  bool IsSessionUsernameAllowed(
      const LoginSessionManager::SessionInfo& session_info,
      const PasswdUserInfo& user_info);
  void AttachProcessToSession(int32_t pid);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string required_username_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string client_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  DestroyedCallback on_destroyed_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_greeter_allowed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  std::unique_ptr<WorkerProcessLauncher> launcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::string current_session_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<uid_t> current_uid_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::AssociatedReceiver<mojom::DesktopSessionRequestHandler>
      desktop_session_request_handler_ GUARDED_BY_CONTEXT(sequence_checker_){
          this};
  mojo::AssociatedRemote<mojom::DesktopProcessControl> desktop_process_control_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DesktopSessionLinux> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_SESSION_LINUX_H_
