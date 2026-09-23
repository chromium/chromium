// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_
#define REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_

#include <sys/types.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_backend.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
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
  struct ActiveSessionEntry {
    std::string username;
    base::WeakPtr<DesktopSessionLinux> desktop_session;
  };

  // Validates `options` against `routing_config_` and active sessions. Returns
  // the resolved Linux username on success, or an error message on failure.
  base::expected<std::string, std::string> ValidateClientSession(
      const mojom::DesktopSessionOptions& options) const;
  void OnCreateDbusConnectionResult(
      Callback callback,
      base::expected<GDBusConnectionRef, Loggable> result);
  void OnListUserSessionsResult(
      int terminal_id,
      std::string username,
      base::expected<std::vector<LoginSessionManager::SessionInfo>, Loggable>
          result);
  void OnSessionRemoved(std::string session_id,
                        gvariant::ObjectPath object_path);
  void RemoveDesktopSession(int terminal_id);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SessionRoutingConfig routing_config_ GUARDED_BY_CONTEXT(sequence_checker_);
  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<LoginSessionManager> login_session_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      session_removed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<int /*terminal_id*/, ActiveSessionEntry> desktop_sessions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<UserDesktopSessionBackend> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_USER_DESKTOP_SESSION_BACKEND_H_
