// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_impl.h"

#import <optional>
#import <string_view>
#import <utility>
#import <vector>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/sequence_bound.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record_store.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#pragma mark - Public

DownloadRecordServiceImpl::DownloadRecordServiceImpl(
    const base::FilePath& profile_path)
    : pagination_enabled_(IsDownloadListPaginationEnabled()),
      database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      store_(database_task_runner_, pagination_enabled_) {
  CHECK(IsDownloadListEnabled());
  CHECK(!profile_path.empty());

  // Initialize the DB, then run the matching startup cleanup. Both calls
  // dispatch on `database_task_runner_` via `AsyncCall`, which preserves
  // ordering.
  store_.AsyncCall(&DownloadRecordStore::InitializeDatabase)
      .WithArgs(profile_path);
  if (pagination_enabled_) {
    store_.AsyncCall(&DownloadRecordStore::MarkUnfinishedDownloadsAsFailed);
  } else {
    store_.AsyncCall(&DownloadRecordStore::LoadHistoricalRecords);
  }
}

DownloadRecordServiceImpl::~DownloadRecordServiceImpl() = default;

void DownloadRecordServiceImpl::RecordDownload(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord record = DownloadRecord(task);
  // Stamp creation time on first record.
  record.created_time = base::Time::Now();

  store_.AsyncCall(&DownloadRecordStore::InsertRecord)
      .WithArgs(record)
      .Then(base::BindOnce(&DownloadRecordServiceImpl::OnRecordInserted,
                           weak_ptr_factory_.GetWeakPtr(), task->GetWeakPtr(),
                           record));
}

void DownloadRecordServiceImpl::OnRecordInserted(
    base::WeakPtr<web::DownloadTask> weak_task,
    const DownloadRecord& record,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // `weak_task` is a `WeakPtr` because `task` may be destroyed mid-flight
  // (e.g. user cancels -> `CleanupCurrentDownload` -> `task_.reset()`). We
  // are not yet observing `task`, so no `OnDownloadDestroyed` fires. Skip
  // when null.
  web::DownloadTask* task = weak_task.get();
  if (!success || !task) {
    return;
  }

  // Guard against double-registration: a retried task ("Try Again") may
  // re-enter `RecordDownload` with the same pointer, and `AddObservation`
  // CHECKs uniqueness.
  if (download_task_observations_.IsObservingSource(task)) {
    return;
  }

  download_task_observations_.AddObservation(task);
  NotifyDownloadAdded(record);
}

void DownloadRecordServiceImpl::GetAllDownloadsAsync(
    DownloadRecordsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  store_.AsyncCall(&DownloadRecordStore::GetAllFromCache)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::GetDownloadByIdAsync(
    const std::string& download_id,
    DownloadRecordCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  store_.AsyncCall(&DownloadRecordStore::GetById)
      .WithArgs(download_id)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::RemoveDownloadByIdAsync(
    const std::string& download_id,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Stop observing the task if we still hold it.
  web::DownloadTask* task_to_remove = GetDownloadTaskById(download_id);
  if (task_to_remove) {
    download_task_observations_.RemoveObservation(task_to_remove);
  }

  store_.AsyncCall(&DownloadRecordStore::DeleteRecord)
      .WithArgs(download_id)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             const std::string& download_id, CompletionCallback callback,
             bool success) {
            if (service && success) {
              service->NotifyDownloadsRemoved({std::string_view(download_id)});
            }
            if (callback) {
              std::move(callback).Run(success);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), download_id, std::move(callback)));
}

void DownloadRecordServiceImpl::GetDownloadsPageAsync(
    const DownloadRecordQuery& query,
    DownloadRecordsPageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Paginated readers require kDownloadListPagination.
  DCHECK(pagination_enabled_);
  store_.AsyncCall(&DownloadRecordStore::GetDownloadsPage)
      .WithArgs(query)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::GetDownloadsCountAsync(
    std::optional<DownloadFilterType> filter,
    DownloadRecordsCountCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(pagination_enabled_);

  // Translate the optional filter into a count-only query; cursor fields
  // are ignored by the DB layer for counts.
  DownloadRecordQuery query;
  query.filter_type = filter;

  store_.AsyncCall(&DownloadRecordStore::GetDownloadsCount)
      .WithArgs(query)
      .Then(std::move(callback));
}

void DownloadRecordServiceImpl::UpdateDownloadFilePathAsync(
    const std::string& download_id,
    const base::FilePath& file_path,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  store_.AsyncCall(&DownloadRecordStore::UpdateFilePathInRecord)
      .WithArgs(download_id, file_path)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             CompletionCallback callback,
             std::optional<DownloadRecord> updated_record) {
            bool success = updated_record.has_value();
            if (service && success) {
              service->NotifyDownloadUpdated(updated_record.value());
            }
            if (callback) {
              std::move(callback).Run(success);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

web::DownloadTask* DownloadRecordServiceImpl::GetDownloadTaskById(
    std::string_view download_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Scan observed tasks for a matching identifier.
  for (web::DownloadTask* task : download_task_observations_.sources()) {
    if (base::SysNSStringToUTF8(task->GetIdentifier()) == download_id) {
      return task;
    }
  }
  return nullptr;
}

#pragma mark - Observer Management

void DownloadRecordServiceImpl::AddObserver(DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.AddObserver(observer);
}

void DownloadRecordServiceImpl::RemoveObserver(
    DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.RemoveObserver(observer);
}

#pragma mark - web::DownloadTaskObserver

void DownloadRecordServiceImpl::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord updated_record = DownloadRecord(task);

  store_.AsyncCall(&DownloadRecordStore::UpdateRecord)
      .WithArgs(updated_record)
      .Then(base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             std::optional<DownloadRecord> record_opt) {
            if (service && record_opt.has_value()) {
              service->NotifyDownloadUpdated(record_opt.value());
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void DownloadRecordServiceImpl::OnDownloadDestroyed(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);
  if (pagination_enabled_) {
    // Snapshot the id BEFORE `RemoveObservation`, which may drop the last
    // handle on `task`. `EvictOnDestroy` is FIFO-ordered with every other
    // CRUD task on the DB sequence (see `download_record_store.h`), so a
    // pending `UpdateRecord` for the same id finishes first and cannot
    // resurrect the entry.
    std::string download_id = base::SysNSStringToUTF8(task->GetIdentifier());
    store_.AsyncCall(&DownloadRecordStore::EvictOnDestroy)
        .WithArgs(std::move(download_id));
  }
  download_task_observations_.RemoveObservation(task);
}

#pragma mark - Private

void DownloadRecordServiceImpl::NotifyDownloadAdded(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadAdded, record);
}

void DownloadRecordServiceImpl::NotifyDownloadUpdated(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadUpdated, record);
}

void DownloadRecordServiceImpl::NotifyDownloadsRemoved(
    const std::vector<std::string_view>& download_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadsRemoved, download_ids);
}
