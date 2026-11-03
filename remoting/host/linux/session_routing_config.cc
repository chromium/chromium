// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/session_routing_config.h"

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "remoting/base/errors.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/secure_file_reader.h"
#include "remoting/signaling/signaling_id_util.h"

namespace remoting {

namespace {

constexpr size_t kMaxConfigFileSizeBytes = 64 * 1024;  // 64 KB

constexpr char kSessionUserKey[] = "sessionUser";
constexpr char kCreateRemoteUserSessionsKey[] = "createRemoteUserSessions";

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
  auto content =
      ReadSecureFile(path, expected_owner_uid, kMaxConfigFileSizeBytes);
  if (!content.has_value()) {
    return std::move(content).error().UnexpectedWithContext(
        FROM_HERE, "While reading session routing config");
  }
  if (!content->has_value()) {
    return base::ok(std::nullopt);
  }

  auto json = base::JSONReader::ReadAndReturnValueWithError(
      **content, base::JSON_PARSE_RFC);
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

  const base::Value* session_user_value = dict.Find(kSessionUserKey);
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

  const base::Value* create_remote_value =
      dict.Find(kCreateRemoteUserSessionsKey);
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
    if (key != kSessionUserKey && key != kCreateRemoteUserSessionsKey) {
      LOG(WARNING) << "Unknown key in session routing config: " << key;
    }
  }

  RoutingMode mode = RoutingMode::kRejectAll;
  if (!session_user.empty()) {
    mode = RoutingMode::kSessionUser;
  } else if (is_corp_host) {
    // Corp hosts route connections to the local user matching the client's
    // corp username unless `sessionUser` is set.
    mode = RoutingMode::kMatchCorpClientUsername;
  }

  return base::ok(SessionRoutingConfig(mode, std::move(session_user),
                                       create_remote, is_corp_host));
}

// static
base::expected<void, Loggable> SessionRoutingConfig::WriteToFile(
    const base::FilePath& path,
    std::string_view session_user,
    bool create_remote_user_sessions) {
  // Never write a file that LoadAndValidate() would reject.
  if (!session_user.empty() && !IsValidUsername(session_user)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Not writing session routing config '", path.value(),
                      "': ", base::GetQuotedJSONString(session_user),
                      " is not a valid username."})));
  }

  // Files are created with the effective UID as the owner.
  const uid_t owner_uid = geteuid();

  // Unlike LoadAndValidate(), this fails if the directory doesn't exist.
  auto directory = CheckSecureDirectory(path.DirName(), owner_uid);
  if (!directory.has_value()) {
    return std::move(directory).error().UnexpectedWithContext(
        FROM_HERE, "While writing session routing config");
  }

  base::DictValue dict;
  if (!session_user.empty()) {
    dict.Set(kSessionUserKey, session_user);
  }
  if (create_remote_user_sessions) {
    dict.Set(kCreateRemoteUserSessionsKey, true);
  }
  // Pretty-printed, since users may edit the file by hand.
  std::optional<std::string> json =
      base::WriteJsonWithOptions(dict, base::JSONWriter::OPTIONS_PRETTY_PRINT);
  CHECK(json.has_value());

  // WriteFileAtomically() writes to a temporary file created with mode 0600 in
  // the same directory, then renames it over `path` (replacing rather than
  // following any symlink there). Operating on paths here and in
  // SetPosixFilePermissions() (which follows symlinks) is fine since the
  // directory has just been verified to be owned by `owner_uid` and not
  // writable by group or others, so no one else can replace `path`.
  if (!base::ImportantFileWriter::WriteFileAtomically(path, *json)) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StrCat({"Failed to write '", path.value(), "'."})));
  }
  if (!base::SetPosixFilePermissions(path, 0644)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Failed to set permissions of '", path.value(), "': ",
                      logging::SystemErrorCodeToString(
                          logging::GetLastSystemErrorCode())})));
  }

  // Sanity check: make sure that the daemon will accept the written file.
  auto loaded = LoadAndValidate(path, /*is_corp_host=*/false, owner_uid);
  if (!loaded.has_value()) {
    return std::move(loaded).error().UnexpectedWithContext(
        FROM_HERE, "While verifying the written session routing config");
  }
  if (!loaded->has_value() || (*loaded)->session_user() != session_user ||
      (*loaded)->create_remote_user_sessions() != create_remote_user_sessions) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StrCat({"Session routing config '", path.value(),
                                 "' doesn't match what was written."})));
  }
  return base::ok();
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
      CHECK(is_corp_host_);
      std::string email;
      SplitSignalingIdResource(client_id, &email, /*resource=*/nullptr);
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
