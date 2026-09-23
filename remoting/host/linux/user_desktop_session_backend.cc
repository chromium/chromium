// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/user_desktop_session_backend.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/source_location.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_linux.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/linux/session_routing_config.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace remoting {

// static
const LoginSessionManager::SessionInfo*
UserDesktopSessionBackend::SelectBestGraphicalSession(
    base::span<const LoginSessionManager::SessionInfo> sessions) {
  const LoginSessionManager::SessionInfo* best_session = nullptr;
  std::tuple<int, bool, bool> best_priority = {-1, false, false};

  for (const auto& session : sessions) {
    if (session.session_class != "user" ||
        (session.session_type != "wayland" && session.session_type != "x11") ||
        session.state == "closing") {
      continue;
    }

    // Priority ranking:
    // 1. service == "chrome-remote-desktop-session": dedicated CRD user
    //    session.
    // 2. is_remote == true: prefer remote/headless sessions over local console
    //    sessions to avoid displaying remote user activity on a physical
    //    monitor and ensure compatibility with curtain mode.
    // 3. state == "active": prefer active (foreground) sessions over online
    //    (background) sessions.
    std::tuple<int, bool, bool> priority = {
        session.service == "chrome-remote-desktop-session" ? 1 : 0,
        session.is_remote,
        session.state == "active",
    };

    // `std::tuple::operator>` compares elements lexicographically from left to
    // right (where `true > false` for `bool`), so tier 1 is compared first,
    // followed by tier 2 on ties, and tier 3 on ties. Using strict `>`
    // preserves the first matching session when all three elements are equal.
    if (priority > best_priority) {
      best_priority = priority;
      best_session = &session;
    }
  }

  return best_session;
}

UserDesktopSessionBackend::UserDesktopSessionBackend(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    SessionRoutingConfig routing_config)
    : io_task_runner_(io_task_runner),
      routing_config_(std::move(routing_config)) {}

UserDesktopSessionBackend::~UserDesktopSessionBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UserDesktopSessionBackend::Start(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GDBusConnectionRef::CreateForSystemBus(
      base::BindOnce(&UserDesktopSessionBackend::OnCreateDbusConnectionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<DesktopSession> UserDesktopSessionBackend::CreateDesktopSession(
    int id,
    DaemonProcess* daemon_process,
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto desktop_session = std::make_unique<DesktopSessionLinux>(
      daemon_process, id, options.required_username, options.client_id,
      io_task_runner_,
      base::BindOnce(&UserDesktopSessionBackend::RemoveDesktopSession,
                     weak_ptr_factory_.GetWeakPtr(), id),
      /*is_greeter_allowed=*/false);

  auto resolved_user = ValidateClientSession(options);
  if (!resolved_user.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DesktopSessionLinux::TerminateSession,
                                  desktop_session->GetWeakPtr(),
                                  ErrorCode::SESSION_REJECTED,
                                  resolved_user.error(), FROM_HERE));
    return desktop_session;
  }

  desktop_sessions_[id] =
      ActiveSessionEntry{*resolved_user, desktop_session->GetWeakPtr()};

  if (login_session_manager_) {
    login_session_manager_->ListUserSessions(
        *resolved_user,
        base::BindOnce(&UserDesktopSessionBackend::OnListUserSessionsResult,
                       weak_ptr_factory_.GetWeakPtr(), id, *resolved_user));
  }

  return desktop_session;
}

base::expected<std::string, std::string>
UserDesktopSessionBackend::ValidateClientSession(
    const mojom::DesktopSessionOptions& options) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto resolved_user = routing_config_.ResolveSessionUser(options.client_id);
  if (!resolved_user.has_value()) {
    LOG(ERROR) << "Client " << options.client_id
               << " is not permitted by session routing configuration.";
    return base::unexpected(
        "Client is not permitted by session routing config.");
  }

  if (!options.required_username.empty() &&
      !base::EqualsCaseInsensitiveASCII(options.required_username,
                                        *resolved_user)) {
    LOG(ERROR) << "Resolved session user (" << *resolved_user
               << ") does not match required username ("
               << options.required_username << ").";
    return base::unexpected(
        "Resolved session user does not match required username.");
  }

  // This generally shouldn't happen because DaemonProcess persists desktop
  // sessions across reconnection, but we disconnect in case of unexpected
  // situations.
  for (const auto& [existing_id, entry] : desktop_sessions_) {
    if (entry.desktop_session &&
        base::EqualsCaseInsensitiveASCII(entry.username, *resolved_user)) {
      LOG(ERROR) << "User " << *resolved_user
                 << " already has an active remote desktop session.";
      return base::unexpected(
          "Target user already has an active remote desktop session.");
    }
  }

  return base::ok(*std::move(resolved_user));
}

void UserDesktopSessionBackend::TerminateAllSessions(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // User login sessions are externally managed (or persist across daemon
  // restarts), so we do not terminate the underlying systemd-logind sessions
  // when CRD is shutting down. DaemonProcess destroys the DesktopSessionLinux
  // objects (and their worker processes) during shutdown.
  desktop_sessions_.clear();

  std::move(callback).Run(base::ok());
}

DesktopSession* UserDesktopSessionBackend::GetSessionByUid(uid_t uid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [id, entry] : desktop_sessions_) {
    if (entry.desktop_session && entry.desktop_session->current_uid() == uid) {
      return entry.desktop_session.get();
    }
  }
  return nullptr;
}

void UserDesktopSessionBackend::OnCreateDbusConnectionResult(
    Callback callback,
    base::expected<GDBusConnectionRef, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to connect to system D-Bus: " << result.error();
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  connection_ = std::move(*result);
  login_session_manager_ = std::make_unique<LoginSessionManager>(connection_);
  session_removed_subscription_ =
      login_session_manager_->SubscribeSessionRemoved(
          base::BindRepeating(&UserDesktopSessionBackend::OnSessionRemoved,
                              weak_ptr_factory_.GetWeakPtr()));

  // Start session discovery for any desktop sessions created while the system
  // D-Bus connection was still being initialized.
  for (const auto& [id, entry] : desktop_sessions_) {
    if (entry.desktop_session) {
      login_session_manager_->ListUserSessions(
          entry.username,
          base::BindOnce(&UserDesktopSessionBackend::OnListUserSessionsResult,
                         weak_ptr_factory_.GetWeakPtr(), id, entry.username));
    }
  }

  std::move(callback).Run(base::ok());
}

void UserDesktopSessionBackend::OnListUserSessionsResult(
    int terminal_id,
    std::string username,
    base::expected<std::vector<LoginSessionManager::SessionInfo>, Loggable>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = desktop_sessions_.find(terminal_id);
  if (it == desktop_sessions_.end() || !it->second.desktop_session) {
    return;
  }
  auto desktop_session = it->second.desktop_session;

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to list sessions for user " << username << ": "
               << result.error();
    desktop_session->TerminateSession(
        ErrorCode::INVALID_STATE, "Failed to query systemd-logind sessions.",
        FROM_HERE);
    return;
  }

  const auto* best_session = SelectBestGraphicalSession(*result);
  if (!best_session) {
    if (routing_config_.create_remote_user_sessions()) {
      LOG(WARNING) << "No graphical user session found for " << username
                   << "; createRemoteUserSessions is not yet implemented.";
    }
    LOG(ERROR) << "No graphical user session found for " << username;
    desktop_session->TerminateSession(
        ErrorCode::SESSION_REJECTED,
        "No graphical user session found for user.", FROM_HERE);
    return;
  }

  auto user_info = GetPasswdUserInfo(best_session->username);
  if (!user_info.has_value()) {
    LOG(ERROR) << "Failed to find passwd entry for session user "
               << best_session->username << ": " << user_info.error();
    desktop_session->TerminateSession(ErrorCode::INVALID_STATE,
                                      "Failed to look up user credentials.",
                                      FROM_HERE);
    return;
  }

  if (user_info->uid != best_session->uid) {
    LOG(ERROR) << "UID mismatch between passwd (" << user_info->uid
               << ") and systemd session (" << best_session->uid << ")";
    desktop_session->TerminateSession(ErrorCode::INVALID_STATE,
                                      "Session UID does not match passwd UID.",
                                      FROM_HERE);
    return;
  }

  HOST_LOG << "Attaching desktop session " << terminal_id
           << " to graphical session " << best_session->session_id
           << " (type=" << best_session->session_type
           << ", state=" << best_session->state
           << ", service=" << best_session->service << ") for user "
           << username;
  desktop_session->SetSessionInfo(*best_session, *user_info);
}

void UserDesktopSessionBackend::OnSessionRemoved(
    std::string session_id,
    gvariant::ObjectPath /*object_path*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<base::WeakPtr<DesktopSessionLinux>> sessions_to_terminate;
  for (const auto& [id, entry] : desktop_sessions_) {
    if (entry.desktop_session &&
        entry.desktop_session->current_session_id() == session_id) {
      sessions_to_terminate.push_back(entry.desktop_session);
    }
  }

  for (auto& session : sessions_to_terminate) {
    if (session) {
      session->TerminateSession(ErrorCode::SESSION_REJECTED,
                                "Login session terminated.", FROM_HERE);
    }
  }
}

void UserDesktopSessionBackend::RemoveDesktopSession(int terminal_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  desktop_sessions_.erase(terminal_id);
}

}  // namespace remoting
