// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/download/model/download_record_query.h"

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

  // Page-result callback for pagination reads.
  using DownloadRecordsPageCallback =
      base::OnceCallback<void(std::vector<DownloadRecord>)>;
  // Count callback for pagination reads.
  using DownloadRecordsCountCallback = base::OnceCallback<void(size_t)>;

  DownloadRecordService() = default;

  DownloadRecordService(const DownloadRecordService&) = delete;
  DownloadRecordService& operator=(const DownloadRecordService&) = delete;

  ~DownloadRecordService() override = default;

  // Records a new download and starts observing it.
  virtual void RecordDownload(web::DownloadTask* task) = 0;
  // Retrieves all downloads. `callback` runs on the calling sequence.
  virtual void GetAllDownloadsAsync(DownloadRecordsCallback callback) = 0;
  // Retrieves a download by ID. `callback` runs on the calling sequence.
  virtual void GetDownloadByIdAsync(const std::string& download_id,
                                    DownloadRecordCallback callback) = 0;
  // Removes a download by ID. `callback` runs on the calling sequence.
  virtual void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Returns one keyset-paginated page of download records.
  //
  // Ordering: `(created_time DESC, download_id DESC)`. Stable across pages
  // even when concurrent inserts/updates land on intervening rows.
  //
  // Cursor: pass an empty `query` for the first page; for subsequent pages
  // set `cursor_created_time` / `cursor_download_id` from the last returned
  // row. The cursor is by ordering keys, not row identity, so deleting the
  // cursor row does not invalidate continuation.
  //
  // Filters: `filter_type` selects a file category; `name_query` applies a
  // case-insensitive substring match on the normalized file name.
  //
  // Freshness: rows still in the in-memory active cache (e.g. an in-progress
  // download whose byte-progress update has not been flushed) are returned
  // with the cached value overlaying the persisted row. Incognito records
  // are merged from memory (they are never persisted).
  //
  // Requires `kDownloadListPagination`. `callback` runs on the calling
  // sequence; returns an empty vector if the DB is not yet initialized.
  virtual void GetDownloadsPageAsync(const DownloadRecordQuery& query,
                                     DownloadRecordsPageCallback callback) = 0;

  // Returns the total count of records matching `filter` across persisted
  // rows plus the in-memory incognito set.
  //
  // When `filter` is unset or `kAll`, counts everything. Requires
  // `kDownloadListPagination`. `callback` runs on the calling sequence;
  // returns 0 if the DB is not yet initialized.
  virtual void GetDownloadsCountAsync(
      std::optional<DownloadFilterType> filter,
      DownloadRecordsCountCallback callback) = 0;

  // Updates the file path for a download record by ID. `callback` runs on
  // the calling sequence.
  virtual void UpdateDownloadFilePathAsync(
      const std::string& download_id,
      const base::FilePath& file_path,
      CompletionCallback callback = CompletionCallback()) = 0;

  // Returns the live `web::DownloadTask` for `download_id`, or nullptr if
  // the task has already completed.
  virtual web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const = 0;

  // Observer management.
  virtual void AddObserver(DownloadRecordObserver* observer) = 0;
  virtual void RemoveObserver(DownloadRecordObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
