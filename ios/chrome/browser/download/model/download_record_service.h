// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_

#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace web {
class DownloadTask;
}  // namespace web

struct DownloadRecord;
class DownloadRecordObserver;

// Base class for download record service that defines the public interface.
class DownloadRecordService : public KeyedService {
 public:
  // Callback types for async operations.
  using DownloadRecordsCallback =
      base::OnceCallback<void(std::vector<DownloadRecord>)>;
  using DownloadRecordCallback =
      base::OnceCallback<void(std::optional<DownloadRecord>)>;
  using CompletionCallback = base::OnceCallback<void(bool success)>;

  DownloadRecordService() = default;

  DownloadRecordService(const DownloadRecordService&) = delete;
  DownloadRecordService& operator=(const DownloadRecordService&) = delete;

  ~DownloadRecordService() override = default;

  // Records a new download and start observing it.
  virtual void RecordDownload(web::DownloadTask* task) = 0;
  // Retrieves all downloads. Callback is invoked on the calling thread.
  virtual void GetAllDownloadsAsync(DownloadRecordsCallback callback) = 0;
  // Retrieves a download by ID. Callback is invoked on the calling thread.
  virtual void GetDownloadByIdAsync(const std::string& download_id,
                                    DownloadRecordCallback callback) = 0;
  // Removes a download by ID. Callback is invoked on the calling thread.
  virtual void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Updates the file path for a download record by ID.
  // Callback is invoked on the calling thread.
  virtual void UpdateDownloadFilePathAsync(
      const std::string& download_id,
      const base::FilePath& file_path,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Gets download task by ID.
  virtual web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const = 0;

  // Observer management.
  virtual void AddObserver(DownloadRecordObserver* observer) = 0;
  virtual void RemoveObserver(DownloadRecordObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
