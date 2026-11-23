// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
#define COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"

namespace webrtc_logging {

// Generous upper bound for the log list file size (intentional overshot).
// The index is capped at 50 lines (kLogListLimitLines), so anything near this
// size is corrupt or tampered with.
inline constexpr size_t kMaxLogListFileSizeBytes = 1000000;

enum class ApiType {
  kExtension,
  kWeb,
};

enum class LogListReadResult {
  // Existing regular file was read into |contents|.
  kRead,
  // File does not exist (and is not a symlink).
  kNotFound,
  // Path is a symlink (possibly dangling) or another replaceable non-regular
  // file such as a FIFO; |contents| is left empty so the caller can overwrite
  // it atomically.
  kNonRegularReplaceable,
  // Parent directory is a symlink, path is a directory, or an existing regular
  // file could not be opened/read or exceeded |kMaxLogListFileSizeBytes|;
  // caller should leave the file untouched.
  kError,
};

class TextLogList {
 public:
  // Gets the file path for the log directory in a browser context's directory
  // for a given API type. The directory name will be appended to
  // |browser_context_path| and returned.
  static base::FilePath GetWebRtcLogDirectoryForBrowserContextPath(
      const base::FilePath& browser_context_path,
      ApiType api_type);

  // Gets the file paths for log directories in a browser context's directory
  // for all supported API types. The directory names will be appended to
  // |browser_context_path| and returned.
  static std::vector<base::FilePath>
  GetWebRtcLogDirectoriesForBrowserContextPath(
      const base::FilePath& browser_context_path);

  // Gets the file path for the log list file in a directory. The log list file
  // name will be appended to |dir| and returned.
  static base::FilePath GetWebRtcLogListFileForDirectory(
      const base::FilePath& dir);

  // Reads |log_list_path| into |contents| without following symlinks and
  // without blocking on FIFOs. On POSIX this uses O_NOFOLLOW | O_NONBLOCK. On
  // Windows (where symlink creation is normally privileged), FLAG_NO_FOLLOW has
  // no effect and a racy IsLink() pre-check is the only, best-effort guard.
  static LogListReadResult ReadLogListFile(const base::FilePath& log_list_path,
                                           std::string* contents);

  // Atomically writes |contents| to |log_list_path| after verifying that its
  // parent directory is not a symbolic link.
  static bool WriteLogListFile(const base::FilePath& log_list_path,
                               std::string_view contents);
};

}  // namespace webrtc_logging

#endif  // COMPONENTS_WEBRTC_LOGGING_BROWSER_TEXT_LOG_LIST_H_
