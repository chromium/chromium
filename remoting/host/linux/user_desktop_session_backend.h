// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_
#define REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_

#include <sys/types.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_backend.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/linux/remote_user_desktop_session_creator.h"
#include "remoting/host/linux/session_routing_config.h"
#include "remoting/host/mojom/desktop_session.mojom-forward.h"

namespace remoting {

class DaemonProcess;
class DesktopSessionLinux;

// DesktopSessionBackend implementation that manages desktop sessions for
// local Linux users based on `SessionRoutingConfig`.
class UserDesktopSessionBackend final : public DesktopSessionBackend {
 public:
  UserDesktopSessionBackend(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      SessionRoutingConfig routing_config);
  ~UserDesktopSessionBackend() override;

  UserDesktopSessionBackend(const UserDesktopSessionBackend&) = delete;
  UserDesktopSessionBackend& operator=(const UserDesktopSessionBackend&) =
      delete;

  // DesktopSessionBackend implementation.
  void Start(Callback callback) override;
  std::unique_ptr<DesktopSession> CreateDesktopSession(
      int id,
      DaemonProcess* daemon_process,
      const mojom::DesktopSessionOptions& options) override;
  void TerminateAllSessions(Callback callback) override;
  DesktopSession* GetSessionByUid(uid_t uid) override;

 private:
  friend class UserDesktopSessionBackendTest;

  // Selects the highest-priority graphical session from `sessions`. Returns
  // nullptr if no suitable session is found.
  static const LoginSessionManager::SessionInfo* SelectBestGraphicalSession(
      base::span<const LoginSessionManager::SessionInfo> sessions);

  // Checks whether a remote session may be created for `requested_username`
  // without interactive authentication, given its passwd entry `user_info` and
  // the `valid_shells` from GetValidLoginShells(). Returns an error describing
  // why not otherwise.
  static base::expected<void, Loggable> CheckCreateRemoteSessionUserInfo(
      std::string_view requested_username,
      const PasswdUserInfo& user_info,
      base::span<const base::FilePath> valid_shells);

  // Looks up the passwd entry of `username`, then checks it with
  // CheckCreateRemoteSessionUserInfo() and checks that PAM allows the user to
  // log in. Returns an error describing why not otherwise.
  static base::expected<void, Loggable> CheckCreateRemoteSessionEligibility(
      const std::string& username);

  // A connection is in one of these states:
  // - Looking for the user's graphical session: `creation_timeout` is null and
  //   `desktop_session` has no `current_session_id()`.
  // - Waiting for a session requested via `session_creator_` to show up in
  //   logind: `creation_timeout` is non-null. The user's sessions are looked
  //   up again on each `SessionNew` signal.
  // - Attached: `desktop_session` has a `current_session_id()`.
  struct ActiveSessionEntry {
    std::string username;
    base::WeakPtr<DesktopSessionLinux> desktop_session;
    std::unique_ptr<base::OneShotTimer> creation_timeout;
  };

  // Validates `options` against `routing_config_` and active sessions. Returns
  // the resolved Linux username on success, or an error message on failure.
  base::expected<std::string, std::string> ValidateClientSession(
      const mojom::DesktopSessionOptions& options) const;
  void OnCreateDbusConnectionResult(
      Callback callback,
      base::expected<GDBusConnectionRef, Loggable> result);
  // Looks up the logind sessions of the user of `terminal_id`'s connection.
  void ListUserSessions(int terminal_id);
  void OnListUserSessionsResult(
      int terminal_id,
      base::expected<std::vector<LoginSessionManager::SessionInfo>, Loggable>
          result);

  // Checks that the user of `terminal_id`'s connection is allowed to have a
  // remote session created, then requests one via `session_creator_` and waits
  // for it to show up. Rejects the connection otherwise.
  void CreateRemoteUserSession(int terminal_id);
  void OnCreateSessionResult(int terminal_id,
                             base::expected<void, Loggable> result);
  void OnSessionCreationTimeout(int terminal_id);
  void OnSessionNew(std::string session_id, gvariant::ObjectPath object_path);
  void OnSessionRemoved(std::string session_id,
                        gvariant::ObjectPath object_path);
  void RemoveDesktopSession(int terminal_id);

  // Returns the entry for `terminal_id` if it is waiting for a requested
  // session to show up, or nullptr otherwise.
  ActiveSessionEntry* GetEntryPendingCreation(int terminal_id);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SessionRoutingConfig routing_config_ GUARDED_BY_CONTEXT(sequence_checker_);
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<LoginSessionManager> login_session_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Non-null if `routing_config_.create_remote_user_sessions()` is set and the
  // system D-Bus connection has been established.
  std::unique_ptr<RemoteUserDesktopSessionCreator> session_creator_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      session_new_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      session_removed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<int /*terminal_id*/, ActiveSessionEntry> desktop_sessions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UserDesktopSessionBackend> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_
