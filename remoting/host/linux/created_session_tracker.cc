// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/created_session_tracker.h"

#include <stdint.h>
#include <sys/types.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/secure_file_reader.h"

namespace remoting {

namespace {

// The state file contains a JSON dictionary keyed by session ID:
//
//   {"3": {"username": "alice", "uid": 1000}, "c2": {...}}
//
// `uid` is stored as a JSON number backed by a double rather than an int:
// base::Value ints are signed 32-bit (JSONReader also parses larger integers
// as doubles), but uid_t is unsigned 32-bit, so UIDs above INT_MAX wouldn't
// fit. A double represents every integer up to 2^53 exactly (52-bit
// significand plus the implicit leading bit), so it can hold any uid_t without
// loss. The file is written with OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION so that
// UIDs appear as plain integers.
constexpr char kUsernameKey[] = "username";
constexpr char kUidKey[] = "uid";

constexpr base::FilePath::CharType kStateFileName[] =
    FILE_PATH_LITERAL("created_sessions.json");

// Bounds the amount of data read from the state file. Each entry takes less
// than 360 bytes, so this allows for over 180 entries.
constexpr size_t kMaxStateFileSizeBytes = 64 * 1024;

constexpr size_t kMaxSessionIdLength = 64;
constexpr size_t kMaxUsernameLength = 256;

static_assert(sizeof(uid_t) == sizeof(uint32_t),
              "uid_t is expected to be a 32-bit unsigned integer.");

// (uid_t)-1 is not a valid UID, since it is used as a sentinel value by
// functions like setreuid().
constexpr uid_t kInvalidUid = std::numeric_limits<uid_t>::max();

// logind session IDs are short alphanumeric strings, e.g. "3" or "c2". '_' and
// '-' are also accepted, since they are equally safe to store.
bool IsValidSessionId(std::string_view session_id) {
  return !session_id.empty() && session_id.length() <= kMaxSessionIdLength &&
         std::ranges::all_of(session_id, [](char c) {
           return base::IsAsciiAlphaNumeric(c) || c == '_' || c == '-';
         });
}

// Uses the same character set as SessionRoutingConfig.
bool IsValidUsername(std::string_view username) {
  return !username.empty() && username.length() <= kMaxUsernameLength &&
         username.front() != '-' && std::ranges::all_of(username, [](char c) {
           return base::IsAsciiAlphaNumeric(c) || c == '_' || c == '.' ||
                  c == '-';
         });
}

base::expected<void, Loggable> ValidateEntry(
    const CreatedSessionTracker::Entry& entry) {
  if (!IsValidSessionId(entry.session_id)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Invalid session ID: \"", entry.session_id, "\""})));
  }
  if (!IsValidUsername(entry.username)) {
    return base::unexpected(
        Loggable(FROM_HERE,
                 base::StrCat({"Invalid username: \"", entry.username, "\""})));
  }
  if (entry.uid == kInvalidUid) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Invalid UID: ", base::NumberToString(entry.uid)})));
  }
  return base::ok();
}

// Converts a JSON number to a uid_t. Returns std::nullopt if `value` isn't a
// number, isn't an integer, or is out of range.
std::optional<uid_t> ValueToUid(const base::Value& value) {
  // GetIfDouble() also returns int-typed values as doubles.
  std::optional<double> number = value.GetIfDouble();
  if (!number.has_value() || std::trunc(*number) != *number || *number < 0 ||
      *number >= static_cast<double>(kInvalidUid)) {
    return std::nullopt;
  }
  return static_cast<uid_t>(*number);
}

base::expected<std::vector<CreatedSessionTracker::Entry>, Loggable>
ParseEntries(std::string_view content) {
  auto json = base::JSONReader::ReadAndReturnValueWithError(
      content, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Failed to parse JSON: ", json.error().message})));
  }
  if (!json->is_dict()) {
    return base::unexpected(
        Loggable(FROM_HERE, "Root JSON value is not a dictionary."));
  }

  std::vector<CreatedSessionTracker::Entry> entries;
  entries.reserve(json->GetDict().size());
  for (const auto [session_id, session] : json->GetDict()) {
    if (!session.is_dict()) {
      return base::unexpected(
          Loggable(FROM_HERE, base::StrCat({"Entry for session \"", session_id,
                                            "\" is not a dictionary."})));
    }
    const std::string* username = session.GetDict().FindString(kUsernameKey);
    const base::Value* uid_value = session.GetDict().Find(kUidKey);
    std::optional<uid_t> uid =
        uid_value ? ValueToUid(*uid_value) : std::nullopt;
    if (!username || !uid.has_value()) {
      return base::unexpected(Loggable(
          FROM_HERE, base::StrCat({"Entry for session \"", session_id,
                                   "\" has a missing or invalid field."})));
    }
    CreatedSessionTracker::Entry entry{session_id, *username, *uid};
    auto valid = ValidateEntry(entry);
    if (!valid.has_value()) {
      return base::unexpected(std::move(valid).error());
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::string SerializeEntries(
    const std::vector<CreatedSessionTracker::Entry>& entries) {
  base::DictValue sessions;
  for (const auto& entry : entries) {
    sessions.Set(entry.session_id,
                 base::DictValue()
                     .Set(kUsernameKey, entry.username)
                     .Set(kUidKey, static_cast<double>(entry.uid)));
  }
  std::string json;
  base::JSONWriter::WriteWithOptions(
      sessions, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION, &json);
  return json;
}

}  // namespace

// static
base::FilePath CreatedSessionTracker::GetDefaultFilePath() {
  return GetRunDir().Append(kStateFileName);
}

CreatedSessionTracker::CreatedSessionTracker()
    : CreatedSessionTracker(GetDefaultFilePath(), /*expected_owner_uid=*/0) {}

CreatedSessionTracker::CreatedSessionTracker(base::FilePath file_path,
                                             uid_t expected_owner_uid)
    : file_path_(std::move(file_path)),
      expected_owner_uid_(expected_owner_uid) {
  // The tracker may be created on a different sequence than the one it is
  // used on.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CreatedSessionTracker::~CreatedSessionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::expected<std::vector<CreatedSessionTracker::Entry>, Loggable>
CreatedSessionTracker::Load() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  auto content =
      ReadSecureFile(file_path_, expected_owner_uid_, kMaxStateFileSizeBytes);
  if (!content.has_value()) {
    return base::unexpected(std::move(content).error());
  }
  if (!content->has_value()) {
    return base::ok(std::vector<Entry>());
  }
  auto entries = ParseEntries(**content);
  if (!entries.has_value()) {
    return std::move(entries).error().UnexpectedWithContext(
        FROM_HERE, base::StrCat({"While parsing '", file_path_.value(), "'"}));
  }
  return entries;
}

base::expected<void, Loggable> CreatedSessionTracker::Add(const Entry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  auto valid = ValidateEntry(entry);
  if (!valid.has_value()) {
    return valid;
  }
  // Unlike ReadSecureFile(), this fails if the directory doesn't exist.
  auto directory =
      CheckSecureDirectory(file_path_.DirName(), expected_owner_uid_);
  if (!directory.has_value()) {
    return directory;
  }
  auto entries = LoadForUpdate();
  if (!entries.has_value()) {
    return base::unexpected(std::move(entries).error());
  }
  std::erase_if(*entries, [&entry](const Entry& existing) {
    return existing.session_id == entry.session_id;
  });
  entries->push_back(entry);
  return Save(*entries);
}

base::expected<void, Loggable> CreatedSessionTracker::Remove(
    base::span<const std::string> session_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  auto entries = LoadForUpdate();
  if (!entries.has_value()) {
    return base::unexpected(std::move(entries).error());
  }
  size_t removed = std::erase_if(*entries, [&session_ids](const Entry& entry) {
    return std::ranges::contains(session_ids, entry.session_id);
  });
  if (removed == 0) {
    return base::ok();
  }
  return Save(*entries);
}

base::expected<std::vector<CreatedSessionTracker::Entry>, Loggable>
CreatedSessionTracker::LoadForUpdate() const {
  auto content =
      ReadSecureFile(file_path_, expected_owner_uid_, kMaxStateFileSizeBytes);
  if (!content.has_value()) {
    return base::unexpected(std::move(content).error());
  }
  if (!content->has_value()) {
    return base::ok(std::vector<Entry>());
  }
  auto entries = ParseEntries(**content);
  if (!entries.has_value()) {
    LOG(WARNING) << "Deleting malformed '" << file_path_
                 << "': " << entries.error();
    auto deleted = Save({});
    if (!deleted.has_value()) {
      return base::unexpected(std::move(deleted).error());
    }
    return base::ok(std::vector<Entry>());
  }
  return entries;
}

base::expected<void, Loggable> CreatedSessionTracker::Save(
    const std::vector<Entry>& entries) const {
  if (entries.empty()) {
    if (!base::DeleteFile(file_path_)) {
      return base::unexpected(Loggable(
          FROM_HERE,
          base::StrCat({"Failed to delete '", file_path_.value(), "': ",
                        logging::SystemErrorCodeToString(
                            logging::GetLastSystemErrorCode())})));
    }
    return base::ok();
  }

  // Never write a file that ReadSecureFile() would refuse to read, since that
  // would make all subsequent Add() and Remove() calls fail.
  std::string json = SerializeEntries(entries);
  if (json.size() > kMaxStateFileSizeBytes) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Not writing ", base::NumberToString(entries.size()),
                      " entries to '", file_path_.value(),
                      "', since the file would exceed the size limit (",
                      base::NumberToString(json.size()), " > ",
                      base::NumberToString(kMaxStateFileSizeBytes),
                      " bytes)."})));
  }

  // WriteFileAtomically() writes to a temporary file created with mode 0600 in
  // the same directory, then renames it over `file_path_`. Unlike the read
  // path, this operates on paths rather than on the checked directory FD,
  // which is fine since the directory has just been verified to be owned by
  // `expected_owner_uid_` (root in production) and not writable by others,
  // either by Add() or by ReadSecureFile() in Remove().
  if (!base::ImportantFileWriter::WriteFileAtomically(file_path_, json)) {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"Failed to write '", file_path_.value(), "'."})));
  }
  return base::ok();
}

}  // namespace remoting
