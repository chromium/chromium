// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_display_session_manager.h"

#include <pwd.h>
#include <unistd.h>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/base/switches.h"
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

// RemoteDisplaySession

RemoteDisplaySessionManager::RemoteDisplaySession::RemoteDisplaySession() =
    default;
RemoteDisplaySessionManager::RemoteDisplaySession::RemoteDisplaySession(
    RemoteDisplaySession&&) = default;
RemoteDisplaySessionManager::RemoteDisplaySession::RemoteDisplaySession(
    const RemoteDisplaySession&) = default;
RemoteDisplaySessionManager::RemoteDisplaySession::~RemoteDisplaySession() =
    default;

RemoteDisplaySessionManager::RemoteDisplaySession&
RemoteDisplaySessionManager::RemoteDisplaySession::operator=(
    RemoteDisplaySession&&) = default;
RemoteDisplaySessionManager::RemoteDisplaySession&
RemoteDisplaySessionManager::RemoteDisplaySession::operator=(
    const RemoteDisplaySession&) = default;

// RemoteDisplayInfo

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

// RemoteDisplaySessionManager

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

  auto terminate_all_callback =
      base::BarrierCallback<base::expected<void, Loggable>>(
          it->second.sessions.size(),
          base::BindOnce(
              [](const std::string& display_name, Callback callback,
                 std::vector<base::expected<void, Loggable>> results) {
                for (const auto& result : results) {
                  if (!result.has_value()) {
                    auto loggable = result.error();
                    loggable.AddContext(
                        FROM_HERE,
                        std::string(
                            "Failed to terminate a session for display") +
                            display_name);
                    return;
                  }
                }
                std::move(callback).Run(base::ok());
              },
              std::string(display_name), std::move(callback)));

  for (const auto& [_, session] : it->second.sessions) {
    TerminateRemoteDisplaySession(session, terminate_all_callback);
  }
}

void RemoteDisplaySessionManager::TerminateRemoteDisplaySession(
    const RemoteDisplaySession& session,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(start_state_, StartState::STARTED);

  if (!session.session_info.has_value()) {
    std::move(callback).Run(base::unexpected(
        Loggable(FROM_HERE, "Remote display session " +
                                std::string(session.session_info->session_id) +
                                " has no session info.")));
    return;
  }

  login_session_manager_->TerminateSession(session.session_info->object_path,
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
    const gvariant::ObjectPath& display_path,
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  login_session_manager_->GetSessionInfo(
      session_id,
      base::BindOnce(&RemoteDisplaySessionManager::OnSessionInfoReady,
                     weak_ptr_factory_.GetWeakPtr(), std::string(display_name),
                     display_path));
}

void RemoteDisplaySessionManager::PopulateSessionEnvironment(
    const std::string& display_name,
    const RemoteDisplayInfo& display_info,
    RemoteDisplaySession& session,
    mojom::LoginSessionInfoPtr session_reporter_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(session.session_info.has_value());
  const LoginSessionManager::SessionInfo& session_info = *session.session_info;
  base::EnvironmentMap& env_vars = session.environment_variables;
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
  if (session.user_info.has_value()) {
    env_vars["HOME"] = session.user_info->home_dir.value();
  }
  delegate_->OnRemoteDisplayChanged(display_name, display_info);
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
    auto& display_info = remote_displays_[display_name];
    display_info.sessions[display_path] = RemoteDisplaySession();
    if (!remote_display.session_id.empty()) {
      session_info_queries_blocking_startup_.insert(display_path);
      QuerySessionInfo(display_name, display_path, remote_display.session_id);
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
  auto& display_info = remote_displays_[display_name];
  display_info.sessions[display_path] = RemoteDisplaySession();
  if (!display.session_id.empty()) {
    HOST_LOG << "Querying session info for remote display: "
             << display.remote_id.value()
             << ", session id: " << display.session_id;
    QuerySessionInfo(display_name, display_path, display.session_id);
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
  HOST_LOG << "GDM remote display session removed: " << display_path.value()
           << ", remote id: " << remote_id.value();
  auto it = remote_displays_.find(display_name);
  if (it == remote_displays_.end()) {
    LOG(WARNING) << "Cannot find remote display with name: " << display_name;
    return;
  }
  RemoteDisplayInfo& display_info = it->second;
  display_info.sessions.erase(display_path);
  if (!display_info.sessions.empty()) {
    delegate_->OnRemoteDisplayChanged(display_name, display_info);
    return;
  }
  remote_displays_.erase(it);
  delegate_->OnRemoteDisplayTerminated(display_name);
}

void RemoteDisplaySessionManager::OnRemoteDisplayChanged(
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
           << ", session id: " << display.session_id
           << ", display path: " << display_path.value();
  auto display_it = remote_displays_.find(display_name);
  if (display_it == remote_displays_.end()) {
    LOG(WARNING) << "Cannot find remote display with name: " << display_name;
    return;
  }
  RemoteDisplayInfo& display_info = display_it->second;
  // Find `display_path`. In case it is not found, create the session.
  auto [session_it, inserted] = display_info.sessions.try_emplace(display_path);
  if (inserted) {
    LOG(WARNING) << "Cannot find remote display session with display path: "
                 << display_path.value();
  }
  RemoteDisplaySession& session = session_it->second;
  session.session_info = std::nullopt;
  session.user_info = std::nullopt;
  session.environment_variables.clear();
  if (display.session_id.empty()) {
    delegate_->OnRemoteDisplayChanged(display_name, display_info);
    return;
  }

  QuerySessionInfo(display_name, display_path, display.session_id);
}

void RemoteDisplaySessionManager::OnLoginSessionCreated(
    mojom::LoginSessionInfoPtr session_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name;
  const RemoteDisplayInfo* display_info = nullptr;
  RemoteDisplaySession* session = nullptr;
  for (auto& [d_name, d_info] : remote_displays_) {
    for (auto& [display_path, s] : d_info.sessions) {
      if (s.session_info.has_value() &&
          s.session_info->session_id == session_info->session_id) {
        display_name = d_name;
        display_info = &d_info;
        session = &s;
        break;
      }
    }
  }
  if (!session) {
    HOST_LOG << "Received session info from the login session reporter before "
             << "LoginSessionManager returns its session info. Session ID: "
             << session_info->session_id;
    pending_session_reporter_info_[session_info->session_id] =
        std::move(session_info);
    return;
  }
  PopulateSessionEnvironment(display_name, *display_info, *session,
                             std::move(session_info));
}

void RemoteDisplaySessionManager::OnSessionInfoReady(
    const std::string& display_name,
    const gvariant::ObjectPath& display_path,
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
  auto& session = remote_display_info.sessions[display_path];
  session.session_info = std::move(*result);
  auto user_info_expected = GetPasswdUserInfo(session.session_info->username);
  if (user_info_expected.has_value()) {
    session.user_info = std::move(user_info_expected.value());
  } else {
    LOG(ERROR) << user_info_expected.error();
  }
  if (session.session_info->session_class == "user") {
    FetchSystemdEnvironmentVariables(display_name, display_path,
                                     session.session_info->username);
  } else {
    // TODO: crbug.com/488713023 - poll systemd user environment variables for
    // GNOME 49.
    auto pending_session_reporter_info_it =
        pending_session_reporter_info_.find(session.session_info->session_id);
    if (pending_session_reporter_info_it !=
        pending_session_reporter_info_.end()) {
      mojom::LoginSessionInfoPtr session_reporter_info =
          std::move(pending_session_reporter_info_it->second);
      pending_session_reporter_info_.erase(pending_session_reporter_info_it);
      PopulateSessionEnvironment(display_name, remote_display_info, session,
                                 std::move(session_reporter_info));
    }
  }
  session_info_queries_blocking_startup_.erase(display_path);
  HandleSessionInfoQueriesBlockingStartup();
}

void RemoteDisplaySessionManager::FetchSystemdEnvironmentVariables(
    const std::string& display_name,
    const gvariant::ObjectPath& display_path,
    const std::string& username) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "Failed to get the current executable path.";
    return;
  }
  base::CommandLine command_line(exe_path);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName,
                                 kProcessTypeUserSystemdEnv);
  command_line.AppendSwitchASCII(kSystemdUserEnvUsernameSwitchName, username);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::CommandLine& command_line) {
            std::string output;
            int exit_code;
            if (!base::GetAppOutputWithExitCode(command_line, &output,
                                                &exit_code)) {
              exit_code = -1;  // Launch failure.
            }
            if (exit_code != EXIT_SUCCESS) {
              LOG(ERROR) << "User systemd environment helper process returned "
                            "exit code: "
                         << exit_code;
              return std::string{};
            }
            HOST_LOG << "Successfully fetched systemd environment variable.";
            return output;
          },
          command_line),
      base::BindOnce(
          &RemoteDisplaySessionManager::OnGetUserSystemdEnvironmentResult,
          weak_ptr_factory_.GetWeakPtr(), display_name, display_path));
}

void RemoteDisplaySessionManager::OnGetUserSystemdEnvironmentResult(
    const std::string& display_name,
    const gvariant::ObjectPath& display_path,
    const std::string& output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (output.empty()) {
    // Failed to get environment variables. Logged above.
    return;
  }

  auto remote_display_it = remote_displays_.find(display_name);
  if (remote_display_it == remote_displays_.end()) {
    LOG(WARNING) << "Remote display " << display_name << " not found.";
    return;
  }
  auto& remote_display_info = remote_display_it->second;
  auto& session = remote_display_info.sessions[display_path];
  auto result =
      base::JSONReader::Read(output, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!result.has_value() || !result->is_dict()) {
    LOG(ERROR) << "Failed to parse user systemd environment JSON for display "
               << display_name << ": " << display_path.value();
    return;
  }

  for (auto [key, value] : result->GetDict()) {
    if (!value.is_string()) {
      LOG(WARNING) << "Non-string value in systemd environment for key: "
                   << key;
      continue;
    }
    session.environment_variables[std::move(key)] =
        std::move(value).TakeString();
  }

  delegate_->OnRemoteDisplayChanged(display_name, remote_display_info);
}

}  // namespace remoting
