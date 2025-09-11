// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_

#import <cstdint>
#import <string>

#import "base/files/file_path.h"
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

  bool operator==(const DownloadRecord& other) const;
  bool operator!=(const DownloadRecord& other) const;

  // Compares records excluding progress fields (received_bytes,
  // progress_percent).
  bool EqualsExcludingProgress(const DownloadRecord& other) const;

  // Unique identifier for this download.
  std::string download_id;
  // Original download URL.
  std::string original_url;
  // Redirected URL (empty if no redirection).
  std::string redirected_url;
  // File name including extension.
  std::string file_name;
  // File path where the download is stored (final location).
  base::FilePath file_path;
  // Response path from DownloadTask.
  base::FilePath response_path;
  // Original MIME type from server.
  std::string original_mime_type;
  // Current MIME type (may be different from original).
  std::string mime_type;
  // Content-Disposition header from server.
  std::string content_disposition;
  // Originating host that initiated the download.
  std::string originating_host;
  // HTTP method used for the download.
  std::string http_method;
  // HTTP status code.
  int http_code = -1;
  // Error code if download failed.
  int error_code = 0;
  // Download start time.
  base::Time created_time;
  // Download completion time.
  base::Time completed_time;
  // Bytes downloaded so far.
  int64_t received_bytes = 0;
  // Total bytes to download.
  int64_t total_bytes = 0;
  // Progress percentage (0-100, -1 if unknown).
  int progress_percent = -1;
  // Current download state.
  web::DownloadTask::State state = web::DownloadTask::State::kNotStarted;
  // Whether this download has performed background download.
  bool has_performed_background_download = false;
  // Whether this download is from an incognito session.
  bool is_incognito = false;

 private:
  // Compares all fields between records. Set `include_progress_fields` to false
  // to exclude volatile progress fields (received_bytes, progress_percent).
  bool CompareFields(const DownloadRecord& other,
                     bool include_progress_fields) const;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_H_
