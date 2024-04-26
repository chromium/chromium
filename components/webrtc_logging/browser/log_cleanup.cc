// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc_logging/browser/log_cleanup.h"

#include <stddef.h>

#include <optional>
#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/webrtc_logging/browser/text_log_list.h"

namespace webrtc_logging {

const base::TimeDelta kTimeToKeepLogs = base::Days(5);

namespace {

// Tokenize a line from the log index. Return true/false to indicate if
// the line was valid/invalid. If valid, |capture_time| and |upload_time| will
// be populated with the relevant values. Note that |upload_time| is optional.
bool ReadLineFromIndex(const std::string& line,
                       base::Time* capture_time,
                       std::optional<base::Time>* upload_time) {
  DCHECK(capture_time);
  DCHECK(upload_time);

  // Parse |upload_time|. (May be empty.)
  size_t token_start = 0;
  size_t token_end = line.find(",");
  if (token_end == std::string::npos) {
    return false;
  }
  const bool has_upload_time = (token_end > token_start);
  double upload_time_double;
  if (has_upload_time &&
      !base::StringToDouble(line.substr(token_start, token_end - token_start),
                            &upload_time_double)) {
    return false;
  }

  // Skip |report_id|. (May be empty.)
  token_start = token_end + 1;  // Start beyond the previous token.
  if (token_start >= line.length()) {
    return false;
  }
  token_end = line.find(",", token_start);
  if (token_end == std::string::npos) {
    return false;
  }
  // TODO(crbug.com/41379180): Validate report ID (length and characters).

  // Skip |local_id|. (May be empty.)
  token_start = token_end + 1;  // Start beyond the previous token.
  if (token_start >= line.length()) {
    return false;
  }
  token_end = line.find(",", token_start);
  if (token_end == std::string::npos) {
    return false;
  }
  // TODO(crbug.com/41379180): Validate local ID (length and characters).

  // Parse |capture_time|. (May NOT be empty.)
  token_start = token_end + 1;  // Start beyond the previous token.
  if (token_start >= line.length()) {
    return false;
  }
  token_end = line.length();
  double capture_time_double;
  if (token_end == std::string::npos ||
      !base::StringToDouble(line.substr(token_start, token_end - token_start),
                            &capture_time_double)) {
    return false;
  }

  *capture_time = base::Time::FromSecondsSinceUnixEpoch(capture_time_double);
  *upload_time = has_upload_time
                     ? std::make_optional(base::Time::FromSecondsSinceUnixEpoch(
                           upload_time_double))
                     : std::nullopt;

  return true;
}

// Remove entries of obsolete logs from the log-index.
// * If delete_begin_time.is_max(), older entries are removed and newer ones
//   are retained. The length of time to keep logs is |kTimeToKeepLogs|.
// * If !delete_begin_time.is_max(), logs are deleted within a time range
//   starting at |delete_begin_time| and ending at the present moment.
//   (In practice, we assume no logs were sent back in time from the future,
//   so the actual range is from |delete_begin_time| and until the end of time.)
std::string RemoveObsoleteEntriesFromLogIndex(
    const std::string& log_index,
    const base::Time& delete_begin_time,
    const base::Time& now) {
  std::string new_log_index;

  // Only copy over lines which are (1) valid and (2) not obsolete.
  for (size_t pos = 0; pos < log_index.length();) {
    // Get |pos| to the beginning of the next non-empty line.
    pos = log_index.find_first_not_of("\n", pos);
    if (pos == std::string::npos) {
      break;
    }
    DCHECK_LT(pos, log_index.length());

    size_t line_end = log_index.find("\n", pos);
    DCHECK(line_end == std::string::npos ||
           (pos < line_end && line_end < log_index.length()));
    if (line_end == std::string::npos) {
      line_end = log_index.length();
    }

    const std::string line = log_index.substr(pos, line_end - pos);

    base::Time capture_time;
    std::optional<base::Time> upload_time;
    if (ReadLineFromIndex(line, &capture_time, &upload_time)) {
      bool line_retained;
      if (delete_begin_time.is_max()) {
        // Sentinel value for deleting old files.
        const base::Time older_timestamp =
            upload_time.has_value() ? std::min(capture_time, *upload_time)
                                    : capture_time;
        base::TimeDelta file_age = now - older_timestamp;
        line_retained = (file_age <= kTimeToKeepLogs);
      } else {
        const base::Time newer_timestamp =
            upload_time.has_value() ? std::max(capture_time, *upload_time)
                                    : capture_time;
        line_retained = (newer_timestamp < delete_begin_time);
      }

      if (line_retained) {
        // Only valid and not-to-be-deleted lines will be copied.
        new_log_index += line;
        new_log_index += "\n";
      }
    }

    pos = line_end + 1;
  }

  return new_log_index;
}

}  // namespace

void DeleteOldWebRtcLogFiles(const base::FilePath& log_dir) {
  DeleteOldAndRecentWebRtcLogFiles(log_dir, base::Time::Max());
}

void DeleteOldAndRecentWebRtcLogFiles(const base::FilePath& log_dir,
                                      const base::Time& delete_begin_time) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(log_dir)) {
    // This will happen if no logs have been stored or uploaded.
    DVLOG(3) << "Could not find directory: " << log_dir.value();
    return;
  }

  const base::Time now = base::Time::Now();

  base::FilePath log_list_path =
      TextLogList::GetWebRtcLogListFileForDirectory(log_dir);
  std::string log_list;
  const bool update_log_list = base::PathExists(log_list_path);
  if (update_log_list) {
    constexpr size_t kMaxIndexSizeBytes = 1000000;  // Intentional overshot.
    const bool read_ok = base::ReadFileToStringWithMaxSize(
        log_list_path, &log_list, kMaxIndexSizeBytes);
    if (!read_ok) {
      // If the maximum size was exceeded, updating it will corrupt it. However,
      // the size would not be exceeded unless the user edits it manually.
      LOG(ERROR) << "Couldn't read WebRTC textual logs list (" << log_list_path
                 << ").";
    }
  }

  // Delete relevant logs files (and their associated entries in the index).
  base::FileEnumerator log_files(log_dir, false, base::FileEnumerator::FILES);
  for (base::FilePath name = log_files.Next(); !name.empty();
       name = log_files.Next()) {
    if (name == log_list_path)
      continue;
    base::FileEnumerator::FileInfo file_info(log_files.GetInfo());
    // TODO(crbug.com/40569303): Handle mismatch between timestamps of the .gz
    // file and the .meta file, as well as with the index.
    base::TimeDelta file_age = now - file_info.GetLastModifiedTime();
    if (file_age > kTimeToKeepLogs ||
        (!delete_begin_time.is_max() &&
         file_info.GetLastModifiedTime() > delete_begin_time)) {
      if (!base::DeleteFile(name)) {
        LOG(WARNING) << "Could not delete WebRTC text log file ("
                     << file_info.GetName() << ").";
      }

      // Remove the local ID from the log list file. The ID is guaranteed to be
      // unique.
      std::string id = file_info.GetName().RemoveExtension().MaybeAsASCII();
      size_t id_pos = log_list.find(id);
      if (id_pos == std::string::npos)
        continue;
      log_list.erase(id_pos, id.size());
    }
  }

  if (update_log_list) {
    log_list =
        RemoveObsoleteEntriesFromLogIndex(log_list, delete_begin_time, now);
    bool success = base::WriteFile(log_list_path, log_list);
    DPCHECK(success);
  }
}

}  // namespace webrtc_logging
