// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_

#import <string>

#import "base/time/time.h"
#import "ios/web/public/download/download_task.h"

// Data structure used to record and display information about a download task,
// including file details, progress, and state information.
struct DownloadRecord {
  DownloadRecord();
  explicit DownloadRecord(web::DownloadTask* task);
  DownloadRecord(const DownloadRecord& other);
  DownloadRecord& operator=(const DownloadRecord& other);
  DownloadRecord(DownloadRecord&& other);
  DownloadRecord& operator=(DownloadRecord&& other);
  ~DownloadRecord();

  // Unique identifier for this download.
  std::string download_id;
  // Download URL.
  std::string url;
  // File name including extension.
  std::string file_name;
  // MIME type (e.g., "application/pdf").
  std::string mime_type;
  // Download start time.
  base::Time created_time;
  // Download completion time.
  base::Time completed_time;
  // File size in bytes.
  int64_t file_size = 0;
  // Bytes downloaded so far.
  int64_t received_bytes = 0;
  // Total bytes to download.
  int64_t total_bytes = 0;
  // Progress percentage (0-100, -1 if unknown).
  int progress_percent = -1;
  // Current download state.
  web::DownloadTask::State state = web::DownloadTask::State::kNotStarted;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_
