// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_

#import <map>
#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

namespace base {
class FilePath;
}  // namespace base
class DownloadRecordDatabase;

// Service that manages download records with persistent storage.
class DownloadRecordService : public KeyedService,
                              public web::DownloadTaskObserver {
 public:
  // Callback types for async operations.
  using DownloadRecordsCallback =
      base::OnceCallback<void(std::vector<DownloadRecord>)>;
  using DownloadRecordCallback =
      base::OnceCallback<void(std::optional<DownloadRecord>)>;
  using CompletionCallback = base::OnceCallback<void(bool success)>;

  explicit DownloadRecordService(const base::FilePath& profile_path);

  DownloadRecordService(const DownloadRecordService&) = delete;
  DownloadRecordService& operator=(const DownloadRecordService&) = delete;

  ~DownloadRecordService() override;

  // Records a new download and start observing it.
  void RecordDownload(web::DownloadTask* task);
  // Retrieves all downloads. Callback is invoked on the calling thread.
  void GetAllDownloadsAsync(DownloadRecordsCallback callback);
  // Retrieves a download by ID. Callback is invoked on the calling thread.
  void GetDownloadByIdAsync(const std::string& download_id,
                            DownloadRecordCallback callback);
  // Removes a download by ID. Callback is invoked on the calling thread.
  void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback());

  // Gets download task by ID.
  web::DownloadTask* GetDownloadTaskById(std::string_view download_id) const;

  // Observer management.
  void AddObserver(DownloadRecordObserver* observer);
  void RemoveObserver(DownloadRecordObserver* observer);

  // web::DownloadTaskObserver implementation.
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

 private:
  // Notifies observers.
  void NotifyDownloadAdded(const DownloadRecord& record);
  void NotifyDownloadUpdated(const DownloadRecord& record);
  void NotifyDownloadsRemoved(
      const std::vector<std::string_view>& download_ids);

  // Determines whether a download record update should be persisted to the
  // database by comparing critical fields between the new and cached records.
  bool ShouldPersistUpdate(const DownloadRecord& new_record,
                           const DownloadRecord& cached_record);

  // Initializes database operations, called on database_task_runner_.
  void InitializeDatabase(const base::FilePath& profile_path);
  void LoadHistoricalRecords();
  void CleanupInconsistentStates();

  // Database CRUD operations, called on database_task_runner_.
  bool InsertRecord(const DownloadRecord& record);
  bool UpdateRecord(const DownloadRecord& record);
  bool DeleteRecord(std::string_view id);
  bool UpdateRecordsState(const std::vector<std::string>& download_ids,
                          web::DownloadTask::State new_state);

  // Cache query operations, called on database_task_runner_.
  std::vector<DownloadRecord> GetAllFromCache();
  std::optional<DownloadRecord> GetByIdFromCache(std::string_view id);

  // Database thread data, accessed on database_task_runner_.
  std::map<std::string, DownloadRecord> database_cache_;
  std::unique_ptr<DownloadRecordDatabase> database_;

  // ObserverList for download record changes.
  base::ObserverList<DownloadRecordObserver, /* check_empty= */ true>
      observers_;
  // Observation for download tasks.
  base::ScopedMultiSourceObservation<web::DownloadTask,
                                     web::DownloadTaskObserver>
      download_task_observations_{this};

  // Task runner for database operations.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  // Main thread sequence checker for public API calls.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Database thread sequence checker. Will be bound on first database thread
  // access.
  SEQUENCE_CHECKER(database_sequence_checker_);

  base::WeakPtrFactory<DownloadRecordService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_H_
