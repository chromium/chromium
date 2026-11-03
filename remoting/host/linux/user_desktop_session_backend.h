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
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/created_session_tracker.h"
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
//
// Graphical sessions that this class asks `session_creator_` to create are
// recorded in a CreatedSessionTracker, and are terminated by
// TerminateAllSessions(), i.e. when the daemon process receives SIGTERM. Other
// graphical sessions are never terminated. Since `session_creator_` doesn't
// report the ID of the session it creates, a session is only recorded when a
// connection that was waiting for it finds it. To avoid terminating a session
// that the host didn't create, a session that only appears after the
// connection has stopped waiting (e.g. after the creation timeout or
// TerminateAllSessions()) is not recorded, and therefore won't be terminated.
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

  // Run with the session ID of a tracked session once it has been handled by
  // TerminateAllSessions(). See OnTrackedSessionInfo().
  using TrackedSessionDoneCallback =
      base::RepeatingCallback<void(std::optional<std::string>)>;

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

  // Returns whether `session`, which a connection attached to while waiting
  // for a session requested via `session_creator_`, should be recorded as
  // created by CRD.
  static bool ShouldTrackCreatedSession(
      const LoginSessionManager::SessionInfo& session);

  // Returns whether the tracked `entry` may be terminated, given the current
  // `session` info of the logind session with the same ID. Session IDs may be
  // reused (e.g. after systemd-logind restarts), so this checks that the
  // session is still a remote graphical session of the tracked user.
  static bool ShouldTerminateTrackedSession(
      const CreatedSessionTracker::Entry& entry,
      const LoginSessionManager::SessionInfo& session);

  // Returns whether `error`, returned by LoginSessionManager, means that the
  // requested session doesn't exist.
  static bool IsNoSuchSessionError(std::string_view error);

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

  // Records `session` as created by CRD in `created_session_tracker_`.
  void TrackCreatedSession(const LoginSessionManager::SessionInfo& session);

  // Called by TerminateAllSessions() with the sessions in
  // `created_session_tracker_`. Terminates the ones that still exist and pass
  // ShouldTerminateTrackedSession(), then removes them from the tracker.
  void OnTrackedSessionsLoaded(
      Callback callback,
      base::expected<std::vector<CreatedSessionTracker::Entry>, Loggable>
          entries);

  // `done` is run with the session ID of `entry` if it should be removed from
  // the tracker (i.e. the session was terminated or no longer exists), or with
  // std::nullopt if it should be kept so that a later attempt can retry.
  void OnTrackedSessionInfo(
      CreatedSessionTracker::Entry entry,
      TrackedSessionDoneCallback done,
      base::expected<LoginSessionManager::SessionInfo, Loggable> result);
  void OnTrackedSessionTerminated(std::string session_id,
                                  TrackedSessionDoneCallback done,
                                  base::expected<void, Loggable> result);
  void OnAllTrackedSessionsHandled(
      Callback callback,
      std::vector<std::optional<std::string>> removable_session_ids);

  // Runs `callback` with the error from removing the terminated sessions from
  // the tracker if that failed, and with `termination_result` otherwise. This
  // is static so that `callback` runs even if `this` has been destroyed.
  static void OnTrackedSessionsRemoved(
      Callback callback,
      base::expected<void, Loggable> termination_result,
      base::expected<void, Loggable> remove_result);

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

  // Records the sessions created via `session_creator_`. Lives on a sequence
  // that allows blocking, since it performs file I/O.
  base::SequenceBound<CreatedSessionTracker> created_session_tracker_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UserDesktopSessionBackend> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_
