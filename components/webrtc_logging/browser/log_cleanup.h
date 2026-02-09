// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_LOGGING_BROWSER_LOG_CLEANUP_H_
#define COMPONENTS_WEBRTC_LOGGING_BROWSER_LOG_CLEANUP_H_

#include <vector>

#include "base/time/time.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace webrtc_logging {

extern const base::TimeDelta kTimeToKeepLogs;

// Deletes logs files older that 5 days in all directories in `log_dirs`.
// Updates the log file list for each directory. Must be called on a task runner
// that's allowed to block.
// TODO(crbug.com/41379158): Only call on the same task runner as where writing
// is done.
void DeleteOldWebRtcLogFiles(const std::vector<base::FilePath>& log_dirs);

// Deletes logs files older that 5 days and logs younger than
// |delete_begin_time| in all directories in `log_dirs`. Updates the log file
// list for each directory. If |delete_begin_time| is base::Time::Max(), no
// recent logs will be deleted, and the function is equal to
// DeleteOldWebRtcLogFiles().
// Must be called on a task runner that's allowed to block.
// TODO(crbug.com/41379158): Only call on the same task runner as where writing
// is done.
void DeleteOldAndRecentWebRtcLogFiles(
    const std::vector<base::FilePath>& log_dirs,
    const base::Time& delete_begin_time);

}  // namespace webrtc_logging

#endif  // COMPONENTS_WEBRTC_LOGGING_BROWSER_LOG_CLEANUP_H_
