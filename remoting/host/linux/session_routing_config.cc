// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/session_routing_config.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "remoting/base/errors.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/is_google_email.h"
#include "remoting/base/loggable.h"
#include "remoting/signaling/signaling_id_util.h"

namespace remoting {

namespace {

constexpr size_t kMaxConfigFileSizeBytes = 64 * 1024;  // 64 KB

bool IsValidUsername(std::string_view username) {
  if (username.empty() || username.length() > 256) {
    return false;
  }
  if (username.front() == '-') {
    return false;
  }
  return std::ranges::all_of(username, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
  });
}

}  // namespace

// static
base::FilePath SessionRoutingConfig::GetDefaultConfigFilePath() {
  return GetMultiProcessHostGlobalConfigDir().Append(
      FILE_PATH_LITERAL("session_routing.json"));
}

// static
base::expected<std::optional<SessionRoutingConfig>, Loggable>
SessionRoutingConfig::LoadAndValidate(const base::FilePath& path,
                                      bool is_corp_host,
                                      uid_t expected_owner_uid) {
  base::ScopedFD fd(HANDLE_EINTR(
      open(path.value().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC)));
  if (!fd.is_valid()) {
    int open_errno = errno;
    if (open_errno == ENOENT) {
      return base::ok(std::nullopt);
    }
    if (open_errno == ELOOP) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StringPrintf(
              "Session routing config '%s' is a symbolic link, which is not "
              "allowed.",
              path.value().c_str())));
    }
    return base::unexpected(Loggable(
        FROM_HERE, base::StringPrintf(
                       "Failed to open session routing config '%s': %s",
                       path.value().c_str(),
                       logging::SystemErrorCodeToString(open_errno).c_str())));
  }

  struct stat stat_buf;
  if (fstat(fd.get(), &stat_buf) != 0) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Failed to stat session routing config '%s': %s",
                           path.value().c_str(),
                           logging::SystemErrorCodeToString(errno).c_str())));
  }

  if (!S_ISREG(stat_buf.st_mode)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Session routing config '%s' is not a regular file.",
                           path.value().c_str())));
  }

  if (stat_buf.st_uid != expected_owner_uid) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf(
            "Session routing config '%s' has owner UID %u (expected %u).",
            path.value().c_str(), stat_buf.st_uid, expected_owner_uid)));
  }

  if ((stat_buf.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Session routing config '%s' is writable by group "
                           "or others (mode: 0%o).",
                           path.value().c_str(), stat_buf.st_mode & 07777)));
  }

  if (stat_buf.st_size > static_cast<off_t>(kMaxConfigFileSizeBytes)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Session routing config '%s' exceeds size limit "
                           "(%ld bytes > %zu bytes).",
                           path.value().c_str(),
                           static_cast<long>(stat_buf.st_size),
                           kMaxConfigFileSizeBytes)));
  }

  base::ScopedFILE file(fdopen(fd.get(), "r"));
  if (!file) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Failed to fdopen session routing config '%s': %s",
                           path.value().c_str(),
                           logging::SystemErrorCodeToString(errno).c_str())));
  }
  // `file` now owns the underlying file descriptor.
  std::ignore = fd.release();

  std::string content;
  if (!base::ReadStreamToStringWithMaxSize(file.get(), kMaxConfigFileSizeBytes,
                                           &content)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Failed to read session routing config '%s'.",
                           path.value().c_str())));
  }

  auto json = base::JSONReader::ReadAndReturnValueWithError(
      content, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StringPrintf(
                       "Failed to parse session routing config JSON '%s': %s",
                       path.value().c_str(), json.error().message.c_str())));
  }
  if (!json->is_dict()) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf(
            "Session routing config '%s' root JSON value is not a dictionary.",
            path.value().c_str())));
  }
  const auto& dict = json->GetDict();

  const base::Value* session_user_value = dict.Find("sessionUser");
  std::string session_user;
  if (session_user_value) {
    if (!session_user_value->is_string()) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StringPrintf("Field 'sessionUser' in '%s' must be a string.",
                             path.value().c_str())));
    }
    session_user = session_user_value->GetString();
    if (session_user.empty() || !IsValidUsername(session_user)) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StringPrintf(
              "Field 'sessionUser' in '%s' is not a valid username: '%s'",
              path.value().c_str(), session_user.c_str())));
    }
  }

  const base::Value* match_corp_value = dict.Find("matchCorpClientUsername");
  bool match_corp = false;
  if (match_corp_value) {
    if (!match_corp_value->is_bool()) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StringPrintf(
              "Field 'matchCorpClientUsername' in '%s' must be a boolean.",
              path.value().c_str())));
    }
    match_corp = match_corp_value->GetBool();
  }

  if (!session_user.empty() && match_corp) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Fields 'sessionUser' and 'matchCorpClientUsername' "
                           "in '%s' are mutually exclusive.",
                           path.value().c_str())));
  }

  const base::Value* create_remote_value =
      dict.Find("createRemoteUserSessions");
  bool create_remote = false;
  if (create_remote_value) {
    if (!create_remote_value->is_bool()) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StringPrintf(
              "Field 'createRemoteUserSessions' in '%s' must be a boolean.",
              path.value().c_str())));
    }
    create_remote = create_remote_value->GetBool();
  }

  for (const auto [key, value] : dict) {
    if (key != "sessionUser" && key != "matchCorpClientUsername" &&
        key != "createRemoteUserSessions") {
      LOG(WARNING) << "Unknown key in session routing config: " << key;
    }
  }

  RoutingMode mode = RoutingMode::kRejectAll;
  if (!session_user.empty()) {
    mode = RoutingMode::kSessionUser;
  } else if (match_corp) {
    mode = RoutingMode::kMatchCorpClientUsername;
  }

  return base::ok(SessionRoutingConfig(mode, std::move(session_user),
                                       create_remote, is_corp_host));
}

SessionRoutingConfig::SessionRoutingConfig() = default;

SessionRoutingConfig::SessionRoutingConfig(RoutingMode mode,
                                           std::string session_user,
                                           bool create_remote_user_sessions,
                                           bool is_corp_host)
    : mode_(mode),
      session_user_(std::move(session_user)),
      create_remote_user_sessions_(create_remote_user_sessions),
      is_corp_host_(is_corp_host) {}

SessionRoutingConfig::~SessionRoutingConfig() = default;

SessionRoutingConfig::SessionRoutingConfig(const SessionRoutingConfig&) =
    default;
SessionRoutingConfig& SessionRoutingConfig::operator=(
    const SessionRoutingConfig&) = default;
SessionRoutingConfig::SessionRoutingConfig(SessionRoutingConfig&&) = default;
SessionRoutingConfig& SessionRoutingConfig::operator=(SessionRoutingConfig&&) =
    default;

base::expected<std::string, ErrorCode> SessionRoutingConfig::ResolveSessionUser(
    std::string_view client_id) const {
  switch (mode_) {
    case RoutingMode::kSessionUser:
      return session_user_;

    case RoutingMode::kMatchCorpClientUsername: {
      std::string email;
      SplitSignalingIdResource(client_id, &email, /*resource=*/nullptr);
      // `is_corp_host_` comes from the host type hint in the host config and
      // may be a false negative on older hosts without the hint set, so we use
      // `IsGoogleEmail()` as a fallback. Conversely, `IsGoogleEmail()` only
      // checks for `@google.com` and is not aware of corporate silo domains,
      // which are covered when `is_corp_host_` is true.
      if (!is_corp_host_ && !IsGoogleEmail(email)) {
        LOG(WARNING) << "Client ID is not a Google corp email: " << client_id;
        return base::unexpected(ErrorCode::SESSION_REJECTED);
      }
      auto email_parts = base::SplitStringOnce(email, '@');
      if (!email_parts.has_value() || email_parts->first.empty() ||
          email_parts->second.empty() || email_parts->second.contains('@')) {
        LOG(WARNING) << "Failed to extract valid username from email: "
                     << email;
        return base::unexpected(ErrorCode::SESSION_REJECTED);
      }
      std::string username = base::ToLowerASCII(email_parts->first);
      if (!IsValidUsername(username)) {
        LOG(WARNING) << "Extracted corp username is invalid: " << username;
        return base::unexpected(ErrorCode::SESSION_REJECTED);
      }
      return username;
    }

    case RoutingMode::kRejectAll:
      return base::unexpected(ErrorCode::SESSION_REJECTED);
  }
}

}  // namespace remoting
