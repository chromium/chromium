// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

namespace base {
class FilePath;
}  // namespace base

class DownloadRecordStore;

// Implementation class that manages download records with persistent storage.
class DownloadRecordServiceImpl : public DownloadRecordService,
                                  public web::DownloadTaskObserver {
 public:
  explicit DownloadRecordServiceImpl(const base::FilePath& profile_path);

  DownloadRecordServiceImpl(const DownloadRecordServiceImpl&) = delete;
  DownloadRecordServiceImpl& operator=(const DownloadRecordServiceImpl&) =
      delete;

  ~DownloadRecordServiceImpl() override;

  // DownloadRecordService implementation.
  void RecordDownload(web::DownloadTask* task) override;
  void GetAllDownloadsAsync(DownloadRecordsCallback callback) override;
  void GetDownloadByIdAsync(const std::string& download_id,
                            DownloadRecordCallback callback) override;
  void RemoveDownloadByIdAsync(
      const std::string& download_id,
      CompletionCallback callback = CompletionCallback()) override;
  void GetDownloadsPageAsync(const DownloadRecordQuery& query,
                             DownloadRecordsPageCallback callback) override;
  void GetDownloadsCountAsync(std::optional<DownloadFilterType> filter,
                              DownloadRecordsCountCallback callback) override;
  void UpdateDownloadFilePathAsync(
      const std::string& download_id,
      const base::FilePath& file_path,
      CompletionCallback callback = CompletionCallback()) override;
  web::DownloadTask* GetDownloadTaskById(
      std::string_view download_id) const override;
  void AddObserver(DownloadRecordObserver* observer) override;
  void RemoveObserver(DownloadRecordObserver* observer) override;

  // web::DownloadTaskObserver implementation.
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

 private:
  // Notifies observers.
  void NotifyDownloadAdded(const DownloadRecord& record);
  void NotifyDownloadUpdated(const DownloadRecord& record);
  void NotifyDownloadsRemoved(
      const std::vector<std::string_view>& download_ids);

  // Reply for the async `InsertRecord` write started by `RecordDownload`;
  // runs on the main sequence and starts observing `task` on success.
  void OnRecordInserted(base::WeakPtr<web::DownloadTask> weak_task,
                        const DownloadRecord& record,
                        bool success);

  // Snapshot of `IsDownloadListPaginationEnabled()` sampled at construction
  // so an in-process Finch flip cannot change the path chosen at startup.
  // Pagination-gated paths read this member, not the live function.
  const bool pagination_enabled_;

  // Task runner that owns `store_`. Declared first so it is alive when
  // `store_`'s `SequenceBound` constructor runs (declaration order).
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  // SQLite-backed store plus the in-memory record cache. All CRUD runs on
  // `database_task_runner_` via `AsyncCall`; `SequenceBound` also destroys
  // the store on that sequence, so in-flight DB tasks observe a live store
  // even after `this` is gone on the main thread.
  base::SequenceBound<DownloadRecordStore> store_;

  // TODO(crbug.com/484371187): Investigate if reentrancy can be removed.
  base::ObserverList<
      DownloadRecordObserver,
      /*check_empty=*/true,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observers_;
  base::ScopedMultiSourceObservation<web::DownloadTask,
                                     web::DownloadTaskObserver>
      download_task_observations_{this};

  // Guards public API calls.
  SEQUENCE_CHECKER(main_sequence_checker_);

  base::WeakPtrFactory<DownloadRecordServiceImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_SERVICE_IMPL_H_
