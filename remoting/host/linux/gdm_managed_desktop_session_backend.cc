// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdm_managed_desktop_session_backend.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "remoting/base/async_file_util.h"
#include "remoting/base/branding.h"
#include "remoting/base/errors.h"
#include "remoting/base/logging.h"
#include "remoting/base/source_location.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_linux.h"
#include "remoting/host/linux/remote_display_session_manager.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace remoting {

namespace {

// Delay to throttle writes to the remote displays file. This prevents bursts of
// remote display changes during creation and greeter->user switchover.
constexpr base::TimeDelta kWriteRemoteDisplaysDelay = base::Seconds(5);

std::string GenerateRandomDisplayName() {
  std::string out;
  // GDM does not allow hyphens in the RemoteDisplay remote_id, so we just
  // remove it.
  base::RemoveChars(base::Uuid::GenerateRandomV4().AsLowercaseString(), "-",
                    &out);
  return out;
}

base::FilePath GetRemoteDisplaysConfigFilePath() {
  base::FilePath config_dir = GetConfigDir();
  if (config_dir.empty()) {
    LOG(ERROR) << "Failed to get config directory.";
    return {};
  }
  return config_dir.Append("remote_displays.json");
}

}  // namespace

GdmManagedDesktopSessionBackend::GdmManagedDesktopSessionBackend(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      write_remote_displays_timer_(
          FROM_HERE,
          kWriteRemoteDisplaysDelay,
          base::BindRepeating(
              &GdmManagedDesktopSessionBackend::WriteRemoteDisplaysToFile,
              base::Unretained(this))) {}

GdmManagedDesktopSessionBackend::~GdmManagedDesktopSessionBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GdmManagedDesktopSessionBackend::Start(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_display_session_manager_.Start(
      this,
      base::BindOnce(&GdmManagedDesktopSessionBackend::OnStartResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<DesktopSession>
GdmManagedDesktopSessionBackend::CreateDesktopSession(
    int id,
    DaemonProcess* daemon_process,
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string display_name;
  auto recovered_it = recovered_displays_.find(options.client_id);
  if (recovered_it != recovered_displays_.end()) {
    display_name = std::move(recovered_it->second);
    recovered_displays_.erase(recovered_it);
    HOST_LOG << "Reusing recovered remote display name " << display_name
             << " for " << options.client_id;
  } else {
    display_name = GenerateRandomDisplayName();
  }

  auto desktop_session = std::make_unique<DesktopSessionLinux>(
      daemon_process, id, options.required_username, options.client_id,
      io_task_runner_,
      base::BindOnce(&GdmManagedDesktopSessionBackend::RemoveDesktopSession,
                     weak_ptr_factory_.GetWeakPtr(), display_name),
      /*is_greeter_allowed=*/true);

  if (auto* info =
          remote_display_session_manager_.GetRemoteDisplayInfo(display_name)) {
    // The remote display already exists. Find a ready session and notify, which
    // will launch the desktop process.
    LOG_IF(WARNING, info->sessions.size() > 1)
        << "There are more than one remote display session for the remote "
        << "display " << display_name;
    for (const auto& [path, session] : info->sessions) {
      if (session.session_info.has_value() && session.user_info.has_value()) {
        desktop_session->SetSessionInfo(*session.session_info,
                                        *session.user_info);
        break;
      }
    }
  } else {
    // Note that this code path will be reached if the recovered remote display
    // has been terminated externally. In this case a new remote display with
    // the same display name will be created.
    // TODO: crbug.com/475611769 - Add timeout mechanism for waiting for the
    // desktop session.
    remote_display_session_manager_.CreateRemoteDisplay(
        display_name,
        base::BindOnce(
            &GdmManagedDesktopSessionBackend::OnCreateRemoteDisplayResult,
            weak_ptr_factory_.GetWeakPtr(), display_name));
  }

  desktop_sessions_[display_name] = desktop_session->GetWeakPtr();
  return desktop_session;
}

void GdmManagedDesktopSessionBackend::TerminateAllSessions(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (desktop_sessions_.empty()) {
    std::move(callback).Run(base::ok());
    return;
  }

  std::vector<std::string> display_names;
  for (const auto& [display_name, session] : desktop_sessions_) {
    display_names.push_back(display_name);
  }

  auto barrier = base::BarrierCallback<base::expected<void, Loggable>>(
      display_names.size(),
      base::BindOnce(
          [](Callback callback,
             std::vector<base::expected<void, Loggable>> results) {
            for (auto& result : results) {
              if (!result.has_value()) {
                std::move(callback).Run(
                    base::unexpected(std::move(result).error()));
                return;
              }
            }
            std::move(callback).Run(base::ok());
          },
          std::move(callback)));

  for (const auto& display_name : display_names) {
    remote_display_session_manager_.TerminateRemoteDisplay(display_name,
                                                           barrier);
  }
}

DesktopSession* GdmManagedDesktopSessionBackend::GetSessionByUid(uid_t uid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [display_name, session] : desktop_sessions_) {
    if (!session) {
      continue;
    }
    const auto* info =
        remote_display_session_manager_.GetRemoteDisplayInfo(display_name);
    if (!info) {
      LOG(WARNING) << "  Cannot find remote display info for display name: "
                   << display_name;
      continue;
    }
    for (const auto& [path, remote_session] : info->sessions) {
      if (remote_session.user_info.has_value() &&
          remote_session.user_info->uid == uid) {
        return session.get();
      }
    }
  }
  return nullptr;
}

void GdmManagedDesktopSessionBackend::OnStartResult(
    Callback callback,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  base::FilePath file_path = GetRemoteDisplaysConfigFilePath();
  if (file_path.empty()) {
    // Just don't attempt to recover remote displays in this case.
    std::move(callback).Run(base::ok());
    return;
  }

  ReadFileAsync(
      file_path,
      base::BindOnce(
          &GdmManagedDesktopSessionBackend::OnRemoteDisplaysFileLoaded,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GdmManagedDesktopSessionBackend::OnRemoteDisplaysFileLoaded(
    Callback callback,
    base::FileErrorOr<std::string> load_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_result.has_value()) {
    std::optional<base::Value> json =
        base::JSONReader::Read(*load_result, base::JSON_PARSE_RFC);
    if (json && json->is_dict()) {
      for (const auto [client_id, display_name_value] : json->GetDict()) {
        if (!display_name_value.is_string()) {
          continue;
        }
        const std::string& display_name = display_name_value.GetString();
        if (remote_display_session_manager_.GetRemoteDisplayInfo(
                display_name)) {
          HOST_LOG << "Recovering remote display " << display_name << " for "
                   << client_id;
          // The entry may still be invalid, e.g. if the file has been tampered
          // with such that a user is associated with another user's graphical
          // session. This will be validated in
          // DesktopSessionLinux::SetSessionInfo when the client connects.
          recovered_displays_[client_id] = display_name;
        } else {
          LOG(WARNING) << "Ignored remote display " << display_name
                       << " which no longer exists.";
        }
      }
    } else {
      LOG(ERROR) << "Failed to parse remote displays file.";
    }
  } else if (load_result.error() != base::File::FILE_ERROR_NOT_FOUND) {
    LOG(ERROR) << "Failed to read remote displays file: "
               << base::File::ErrorToString(load_result.error());
  }

  for (const auto& [display_name, info] :
       remote_display_session_manager_.remote_displays()) {
    bool is_recovered = std::ranges::any_of(
        recovered_displays_, [&display_name](const auto& pair) {
          return pair.second == display_name;
        });

    if (!is_recovered) {
      HOST_LOG << "Terminating unknown CRD-managed remote display: "
               << display_name;
      remote_display_session_manager_.TerminateRemoteDisplay(
          display_name,
          base::BindOnce([](base::expected<void, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << result.error();
            }
          }));
    }
  }

  std::move(callback).Run(base::ok());
}

void GdmManagedDesktopSessionBackend::OnCreateRemoteDisplayResult(
    std::string_view display_name,
    base::expected<void, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    // No need to do anything. OnRemoteDisplayChanged() will be called
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

void GdmManagedDesktopSessionBackend::OnRemoteDisplayChanged(
    std::string_view display_name,
    const RemoteDisplaySessionManager::RemoteDisplayInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (info.sessions.empty()) {
    LOG(WARNING) << "Remote display " << display_name << " has no sessions.";
    return;
  }

  if (info.sessions.size() == 1) {
    auto session = FindSession(display_name);
    if (!session) {
      return;
    }
    const auto& remote_session = info.sessions.begin()->second;
    if (remote_session.session_info.has_value() &&
        remote_session.user_info.has_value()) {
      session->SetSessionInfo(*remote_session.session_info,
                              *remote_session.user_info);
    } else {
      session->ClearSessionInfo();
    }
    RequestWriteRemoteDisplaysToFile();
    return;
  }

  // There are multiple sessions with the same display name. This usually
  // happens during greeter->user transition. This is mostly for GRD to do the
  // RDP handover before they terminate the greeter. CRD doesn't do RDP
  // handover, so we just find and terminate the greeter session.
  auto greeter_it = std::ranges::find_if(info.sessions, [](const auto& pair) {
    return pair.second.session_info->session_class == "greeter";
  });
  const RemoteDisplaySessionManager::RemoteDisplaySession* session = nullptr;
  if (greeter_it != info.sessions.end()) {
    session = &greeter_it->second;
    HOST_LOG << "Terminating greeter session "
             << session->session_info->session_id
             << " for remote display: " << display_name;
  } else {
    session = &info.sessions.begin()->second;
    LOG(WARNING) << "Cannot find greeter session. Terminating the first "
                 << session->session_info->session_class << " session "
                 << session->session_info->session_id
                 << " for remote display: " << display_name;
  }
  // We just terminate one session, since terminating multiple sessions at a
  // time will result in multiple OnRemoteDisplayChanged() calls.
  // OnRemoteDisplayChanged() will be called once the session is removed.
  remote_display_session_manager_.TerminateRemoteDisplaySession(
      *session, base::BindOnce([](base::expected<void, Loggable> result) {
        // TODO: crbug.com/475611769 - See what to do with the callback.
        if (!result.has_value()) {
          LOG(ERROR) << result.error();
        }
      }));
}

void GdmManagedDesktopSessionBackend::OnRemoteDisplayTerminated(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto session = FindSession(display_name);
  // session may be nullptr if the desktop session has already been removed by
  // RemoveDesktopSession().
  if (session) {
    session->TerminateSession(ErrorCode::SESSION_REJECTED,
                              "Remote display terminated.", FROM_HERE);
  }
}

void GdmManagedDesktopSessionBackend::RemoveDesktopSession(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  desktop_sessions_.erase(display_name);
  RequestWriteRemoteDisplaysToFile();
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

base::WeakPtr<DesktopSessionLinux> GdmManagedDesktopSessionBackend::FindSession(
    std::string_view display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = desktop_sessions_.find(display_name);
  return it == desktop_sessions_.end() ? nullptr : it->second;
}

void GdmManagedDesktopSessionBackend::RequestWriteRemoteDisplaysToFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This either starts or delays the timer.
  write_remote_displays_timer_.Reset();
}

void GdmManagedDesktopSessionBackend::WriteRemoteDisplaysToFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::DictValue dict;
  for (const auto& [display_name, session] : desktop_sessions_) {
    if (!session) {
      continue;
    }
    const auto* info =
        remote_display_session_manager_.GetRemoteDisplayInfo(display_name);
    if (!info) {
      continue;
    }

    // Only write a remote display with a user session. There is no value to
    // recover a greeter session, and it is not recoverable on GNOME 48 or older
    // since we can't launch the session reporter process ourselves.
    bool has_user_session =
        std::ranges::any_of(info->sessions, [](const auto& pair) {
          return pair.second.session_info.has_value() &&
                 pair.second.session_info->session_class == "user";
        });
    if (!has_user_session) {
      continue;
    }

    if (dict.contains(session->client_id())) {
      LOG(WARNING) << "Multiple remote displays found for "
                   << session->client_id() << ". Using "
                   << *dict.FindString(session->client_id()) << " and ignoring "
                   << display_name;
    } else {
      dict.Set(session->client_id(), display_name);
    }
  }

  base::FilePath file_path = GetRemoteDisplaysConfigFilePath();
  if (file_path.empty()) {
    return;
  }

  std::string json_output;
  if (!base::JSONWriter::Write(dict, &json_output)) {
    LOG(ERROR) << "Failed to serialize remote displays to JSON.";
    return;
  }

  // Only root is allowed to read or write this file.
  WriteFileWithPermissionsAsync(
      file_path, std::move(json_output), 0o600,
      base::BindOnce(
          [](const base::FilePath& path, base::FileErrorOr<void> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to write remote displays to " << path
                         << ": " << base::File::ErrorToString(result.error());
            }
          },
          file_path));
}

}  // namespace remoting
