// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_display_session_manager.h"

#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
#include "remoting/host/linux/passwd_utils.h"

namespace remoting {

namespace {

constexpr std::string_view kRemoteDisplayIdPrefix =
    "/com/google/ChromeRemoteDesktop/RemoteDisplays/";

std::string GetRemoteDisplayName(const gvariant::ObjectPath& remote_id) {
  if (!base::StartsWith(remote_id.value(), kRemoteDisplayIdPrefix)) {
    return {};
  }
  return remote_id.value().substr(kRemoteDisplayIdPrefix.size());
}

}  // namespace

RemoteDisplaySessionManager::RemoteDisplayInfo::RemoteDisplayInfo() = default;
RemoteDisplaySessionManager::RemoteDisplayInfo::RemoteDisplayInfo(
    RemoteDisplayInfo&&) = default;
RemoteDisplaySessionManager::RemoteDisplayInfo::RemoteDisplayInfo(
    const RemoteDisplayInfo&) = default;
RemoteDisplaySessionManager::RemoteDisplayInfo::~RemoteDisplayInfo() = default;

RemoteDisplaySessionManager::RemoteDisplayInfo&
RemoteDisplaySessionManager::RemoteDisplayInfo::operator=(RemoteDisplayInfo&&) =
    default;
RemoteDisplaySessionManager::RemoteDisplayInfo&
RemoteDisplaySessionManager::RemoteDisplayInfo::operator=(
    const RemoteDisplayInfo&) = default;

RemoteDisplaySessionManager::RemoteDisplaySessionManager() = default;
RemoteDisplaySessionManager::~RemoteDisplaySessionManager() = default;

void RemoteDisplaySessionManager::Start(Delegate* delegate, Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(start_state_, StartState::NOT_STARTED);
  DCHECK(delegate);
  DCHECK(callback);

  start_state_ = StartState::STARTING;
  delegate_ = delegate;

  init_callback_ = std::move(callback);
  GDBusConnectionRef::CreateForSystemBus(
      base::BindOnce(&RemoteDisplaySessionManager::OnCreateDbusConnectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDisplaySessionManager::CreateRemoteDisplay(
    std::string_view display_name,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(start_state_, StartState::STARTED);

  auto result = gvariant::ObjectPath::TryFrom(
      std::string(kRemoteDisplayIdPrefix) + std::string(display_name));
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }
  remote_display_manager_.CreateRemoteDisplay(*result, std::move(callback));
}

void RemoteDisplaySessionManager::TerminateRemoteDisplay(
    std::string_view display_name,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(start_state_, StartState::STARTED);

  const auto it = remote_displays_.find(display_name);
  if (it == remote_displays_.end()) {
    std::move(callback).Run(base::unexpected(Loggable(
        FROM_HERE,
        "Remote display " + std::string(display_name) + " not found.")));
    return;
  }

  if (!it->second.session_info.has_value()) {
    std::move(callback).Run(base::unexpected(Loggable(
        FROM_HERE,
        "Remote display " + std::string(display_name) + " has no session.")));
    return;
  }

  login_session_manager_->TerminateSession(it->second.session_info->object_path,
                                           std::move(callback));
}

const RemoteDisplaySessionManager::RemoteDisplayInfo*
RemoteDisplaySessionManager::GetRemoteDisplayInfo(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = remote_displays_.find(display_name);
  if (it == remote_displays_.end()) {
    return nullptr;
  }
  return &it->second;
}

void RemoteDisplaySessionManager::QuerySessionInfo(
    const std::string& display_name,
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  login_session_manager_->GetSessionInfo(
      session_id,
      base::BindOnce(&RemoteDisplaySessionManager::OnSessionInfoReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::string(display_name)));
}

void RemoteDisplaySessionManager::PopulateSessionEnvironment(
    const std::string& display_name,
    RemoteDisplayInfo& display_info,
    mojom::LoginSessionInfoPtr session_reporter_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(display_info.session_info.has_value());
  const LoginSessionManager::SessionInfo& session_info =
      *display_info.session_info;
  base::EnvironmentMap& env_vars = display_info.environment_variables;
  DCHECK(env_vars.empty());
  DCHECK_EQ(session_info.session_id, session_reporter_info->session_id);
  env_vars["XDG_CURRENT_DESKTOP"] = session_reporter_info->xdg_current_desktop;
  env_vars["DBUS_SESSION_BUS_ADDRESS"] =
      session_reporter_info->dbus_session_bus_address;
  env_vars["DISPLAY"] = session_reporter_info->display;
  env_vars["WAYLAND_DISPLAY"] = session_reporter_info->wayland_display;
  env_vars["XDG_SESSION_CLASS"] = session_info.session_class;
  env_vars["XDG_SESSION_TYPE"] = session_info.session_type;
  env_vars["USER"] = session_info.username;
  env_vars["LOGNAME"] = session_info.username;
  // This is the path of XDG_RUNTIME_DIR for all modern Linux systems using
  // systemd.
  env_vars["XDG_RUNTIME_DIR"] =
      base::StringPrintf("/run/user/%d", session_info.uid);
  if (display_info.user_info.has_value()) {
    env_vars["HOME"] = display_info.user_info->home_dir.value();
  }
  delegate_->OnRemoteDisplaySessionChanged(display_name, display_info);
}

void RemoteDisplaySessionManager::HandleSessionInfoQueriesBlockingStartup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (start_state_ == StartState::STARTED) {
    return;
  }

  DCHECK_EQ(start_state_, StartState::STARTING);
  if (session_info_queries_blocking_startup_.empty()) {
    start_state_ = StartState::STARTED;
    std::move(init_callback_).Run(base::ok());
  }
}

void RemoteDisplaySessionManager::OnCreateDbusConnectionResult(
    base::expected<GDBusConnectionRef, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    start_state_ = StartState::NOT_STARTED;
    std::move(init_callback_).Run(base::unexpected(std::move(result).error()));
    return;
  }

  connection_ = std::move(*result);
  login_session_manager_ = std::make_unique<LoginSessionManager>(connection_);
  remote_display_manager_.Init(
      connection_, this,
      base::BindOnce(
          &RemoteDisplaySessionManager::OnGdmRemoteDisplayManagerStarted,
          weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDisplaySessionManager::OnGdmRemoteDisplayManagerStarted(
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    start_state_ = StartState::NOT_STARTED;
    std::move(init_callback_).Run(base::unexpected(std::move(result).error()));
    return;
  }

  login_session_reporter_server_.StartServer();
  for (const auto& [display_path, remote_display] :
       remote_display_manager_.remote_displays()) {
    std::string display_name = GetRemoteDisplayName(remote_display.remote_id);
    if (display_name.empty()) {
      VLOG(1) << "Ignoring unrelated remote display: "
              << remote_display.remote_id.value();
      continue;
    }
    remote_displays_[display_name] = RemoteDisplayInfo();

    if (!remote_display.session_id.empty()) {
      session_info_queries_blocking_startup_.insert(display_name);
      QuerySessionInfo(display_name, remote_display.session_id);
    }
  }
  HandleSessionInfoQueriesBlockingStartup();
}

void RemoteDisplaySessionManager::OnRemoteDisplayCreated(
    const gvariant::ObjectPath& display_path,
    const GdmRemoteDisplayManager::RemoteDisplay& display) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name = GetRemoteDisplayName(display.remote_id);
  if (display_name.empty()) {
    VLOG(1) << "Ignoring unrelated remote display: "
            << display.remote_id.value();
    return;
  }
  if (remote_displays_.contains(display_name)) {
    // When a remote display's session change, GDM re-creates the remote display
    // object instead of just changing the session ID.
    HOST_LOG << "Replacing existing remote display: "
             << display.remote_id.value();
  } else {
    HOST_LOG << "Remote display created: " << display.remote_id.value();
  }
  remote_displays_[display_name] = RemoteDisplayInfo();

  if (!display.session_id.empty()) {
    HOST_LOG << "Querying session info for remote display: "
             << display.remote_id.value()
             << ", session id: " << display.session_id;
    QuerySessionInfo(display_name, display.session_id);
  }
}

void RemoteDisplaySessionManager::OnRemoteDisplayRemoved(
    const gvariant::ObjectPath& display_path,
    const gvariant::ObjectPath& remote_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name = GetRemoteDisplayName(remote_id);
  if (display_name.empty()) {
    VLOG(1) << "Ignoring unrelated remote display: " << remote_id.value();
    return;
  }
  HOST_LOG << "Remote display removed: " << remote_id.value();
  auto it = remote_displays_.find(display_name);
  if (it == remote_displays_.end()) {
    LOG(WARNING) << "Cannot find remote display with name: " << display_name;
    return;
  }
  remote_displays_.erase(it);
  delegate_->OnRemoteDisplayTerminated(display_name);
}

void RemoteDisplaySessionManager::OnRemoteDisplaySessionChanged(
    const gvariant::ObjectPath& display_path,
    const GdmRemoteDisplayManager::RemoteDisplay& display) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name = GetRemoteDisplayName(display.remote_id);
  if (display_name.empty()) {
    VLOG(1) << "Ignoring unrelated remote display: "
            << display.remote_id.value();
    return;
  }
  HOST_LOG << "Remote display session changed: " << display.remote_id.value()
           << ", session id: " << display.session_id;
  auto it = remote_displays_.find(display_name);
  if (it == remote_displays_.end()) {
    LOG(WARNING) << "Cannot find remote display with name: " << display_name;
    return;
  }
  RemoteDisplayInfo& display_info = it->second;
  display_info.session_info = std::nullopt;
  display_info.user_info = std::nullopt;
  display_info.environment_variables.clear();
  if (display.session_id.empty()) {
    delegate_->OnRemoteDisplaySessionChanged(display_name, display_info);
    return;
  }

  QuerySessionInfo(display_name, display.session_id);
}

void RemoteDisplaySessionManager::OnLoginSessionCreated(
    mojom::LoginSessionInfoPtr session_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it =
      std::ranges::find_if(remote_displays_, [&session_info](const auto& it) {
        return it.second.session_info.has_value() &&
               it.second.session_info->session_id == session_info->session_id;
      });
  if (it == remote_displays_.end()) {
    HOST_LOG << "Received session info from the login session reporter before "
             << "LoginSessionManager returns its session info. Session ID: "
             << session_info->session_id;
    pending_session_reporter_info_[session_info->session_id] =
        std::move(session_info);
    return;
  }
  PopulateSessionEnvironment(/*display_name=*/it->first, it->second,
                             std::move(session_info));
}

void RemoteDisplaySessionManager::OnSessionInfoReady(
    const std::string& display_name,
    base::expected<LoginSessionManager::SessionInfo, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to get session info for " << display_name << ": "
               << result.error();
    return;
  }
  DCHECK(result->is_remote);

  auto remote_display_it = remote_displays_.find(display_name);
  DCHECK(remote_display_it != remote_displays_.end());
  auto& remote_display_info = remote_display_it->second;
  remote_display_info.session_info = std::move(*result);
  auto user_info_expected =
      GetPasswdUserInfo(remote_display_info.session_info->username);
  if (user_info_expected.has_value()) {
    remote_display_info.user_info = std::move(user_info_expected.value());
  } else {
    LOG(ERROR) << user_info_expected.error();
  }
  auto pending_session_reporter_info_it = pending_session_reporter_info_.find(
      remote_display_info.session_info->session_id);
  if (pending_session_reporter_info_it !=
      pending_session_reporter_info_.end()) {
    mojom::LoginSessionInfoPtr session_reporter_info =
        std::move(pending_session_reporter_info_it->second);
    pending_session_reporter_info_.erase(pending_session_reporter_info_it);
    PopulateSessionEnvironment(display_name, remote_display_info,
                               std::move(session_reporter_info));
  }
  session_info_queries_blocking_startup_.erase(display_name);
  HandleSessionInfoQueriesBlockingStartup();
}

}  // namespace remoting
