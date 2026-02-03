// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_display_session_manager.h"

#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"

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

std::string GetHomeDirPathForUsername(const std::string& username) {
  struct passwd pw;
  struct passwd* result;
  constexpr int kDefaultPwnameLength = 1024;
  long user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (user_name_length == -1) {
    user_name_length = kDefaultPwnameLength;
  }
  std::vector<char> buffer(user_name_length);
  int err =
      getpwnam_r(username.c_str(), &pw, buffer.data(), buffer.size(), &result);
  if (err != 0) {
    PLOG(ERROR) << "getpwnam_r failed";
    return {};
  }
  if (result == nullptr) {
    LOG(ERROR) << "User not found: " << username;
    return {};
  }
  return std::string(pw.pw_dir);
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

  start_state_ = StartState::STARTING;
  delegate_ = delegate;

  GDBusConnectionRef::CreateForSystemBus(
      base::BindOnce(&RemoteDisplaySessionManager::OnCreateDbusConnectionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    LOG(ERROR) << "Remote display " << display_name << " not found.";
    return;
  }

  if (!it->second.session_info.has_value()) {
    LOG(ERROR) << "Remote display " << display_name << " has no session.";
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
  env_vars["HOME"] = GetHomeDirPathForUsername(session_info.username);
  delegate_->OnRemoteDisplaySessionChanged(display_name, display_info);
}

void RemoteDisplaySessionManager::OnCreateDbusConnectionResult(
    Callback callback,
    base::expected<GDBusConnectionRef, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    start_state_ = StartState::NOT_STARTED;
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  connection_ = std::move(*result);
  login_session_manager_ = std::make_unique<LoginSessionManager>(connection_);
  remote_display_manager_.Init(
      connection_, this,
      base::BindOnce(
          &RemoteDisplaySessionManager::OnGdmRemoteDisplayManagerStarted,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RemoteDisplaySessionManager::OnGdmRemoteDisplayManagerStarted(
    Callback callback,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    start_state_ = StartState::NOT_STARTED;
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  login_session_reporter_server_.StartServer();
  start_state_ = StartState::STARTED;
  std::move(callback).Run(base::ok());
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
  HOST_LOG << "Remote display created: " << display.remote_id.value();
  DCHECK(!remote_displays_.contains(display_name));
  remote_displays_[display_name] = RemoteDisplayInfo();

  if (!display.session_id.empty()) {
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
  remote_display_it->second.session_info = std::move(*result);
  auto pending_session_reporter_info_it = pending_session_reporter_info_.find(
      remote_display_it->second.session_info->session_id);
  if (pending_session_reporter_info_it !=
      pending_session_reporter_info_.end()) {
    mojom::LoginSessionInfoPtr session_reporter_info =
        std::move(pending_session_reporter_info_it->second);
    pending_session_reporter_info_.erase(pending_session_reporter_info_it);
    PopulateSessionEnvironment(display_name, remote_display_it->second,
                               std::move(session_reporter_info));
  }
}

}  // namespace remoting
