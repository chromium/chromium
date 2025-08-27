// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_database.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

#pragma mark - Public

DownloadRecordService::DownloadRecordService(const base::FilePath& profile_path)
    : database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  CHECK(IsDownloadListEnabled());
  CHECK(!profile_path.empty());

  // Detaches database sequence checker so it can bind to the DB thread later.
  DETACH_FROM_SEQUENCE(database_sequence_checker_);

  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<DownloadRecordService> service,
                        const base::FilePath& profile_path) {
                       if (!service) {
                         return;
                       }
                       service->InitializeDatabase(profile_path);
                       service->LoadHistoricalRecords();
                     },
                     weak_ptr_factory_.GetWeakPtr(), profile_path));
}

DownloadRecordService::~DownloadRecordService() {
  // Ensure database is destroyed on the correct thread.
  if (database_) {
    database_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
  }
}

void DownloadRecordService::RecordDownload(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord record = DownloadRecord(task);
  // Sets creation time when we first record the download.
  record.created_time = base::Time::Now();

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordService::InsertRecord,
                     base::Unretained(this), record),
      base::BindOnce(
          [](base::WeakPtr<DownloadRecordService> service,
             web::DownloadTask* task, const DownloadRecord& record,
             bool success) {
            if (service && success) {
              service->download_task_observations_.AddObservation(task);
              service->NotifyDownloadAdded(record);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), task, record));
}

void DownloadRecordService::GetAllDownloadsAsync(
    DownloadRecordsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordService::GetAllFromCache,
                     base::Unretained(this)),
      std::move(callback));
}

void DownloadRecordService::GetDownloadByIdAsync(
    const std::string& download_id,
    DownloadRecordCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordService::GetByIdFromCache,
                     base::Unretained(this), download_id),
      std::move(callback));
}

void DownloadRecordService::RemoveDownloadByIdAsync(
    const std::string& download_id,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Find and remove from observed tasks if present.
  web::DownloadTask* task_to_remove = GetDownloadTaskById(download_id);
  if (task_to_remove) {
    download_task_observations_.RemoveObservation(task_to_remove);
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordService::DeleteRecord,
                     base::Unretained(this), download_id),
      base::BindOnce(
          [](base::WeakPtr<DownloadRecordService> service,
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

web::DownloadTask* DownloadRecordService::GetDownloadTaskById(
    std::string_view download_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Search through all observed download tasks.
  for (web::DownloadTask* task : download_task_observations_.sources()) {
    if (base::SysNSStringToUTF8(task->GetIdentifier()) == download_id) {
      return task;
    }
  }
  return nullptr;
}

#pragma mark - Observer Management

void DownloadRecordService::AddObserver(DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.AddObserver(observer);
}

void DownloadRecordService::RemoveObserver(DownloadRecordObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.RemoveObserver(observer);
}

#pragma mark - web::DownloadTaskObserver

void DownloadRecordService::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord updated_record = DownloadRecord(task);

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordService::UpdateRecord,
                     base::Unretained(this), updated_record),
      base::BindOnce(
          [](base::WeakPtr<DownloadRecordService> service,
             const DownloadRecord& record, bool success) {
            if (service && success) {
              service->NotifyDownloadUpdated(record);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), updated_record));
}

void DownloadRecordService::OnDownloadDestroyed(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  download_task_observations_.RemoveObservation(task);
}

#pragma mark - Private

void DownloadRecordService::NotifyDownloadAdded(const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadAdded, record);
}

void DownloadRecordService::NotifyDownloadUpdated(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadUpdated, record);
}

void DownloadRecordService::NotifyDownloadsRemoved(
    const std::vector<std::string_view>& download_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  observers_.Notify(&DownloadRecordObserver::OnDownloadsRemoved, download_ids);
}

#pragma mark - Database Thread Operations

bool DownloadRecordService::ShouldPersistUpdate(
    const DownloadRecord& new_record,
    const DownloadRecord& cached_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  // Persist only if critical fields have changed.
  // Progress fields (received_bytes, progress_percent) are not persisted to
  // database.
  return new_record.state != cached_record.state ||
         new_record.file_path != cached_record.file_path ||
         new_record.response_path != cached_record.response_path ||
         new_record.completed_time != cached_record.completed_time ||
         new_record.http_code != cached_record.http_code ||
         new_record.error_code != cached_record.error_code ||
         new_record.original_url != cached_record.original_url ||
         new_record.redirected_url != cached_record.redirected_url ||
         new_record.file_name != cached_record.file_name ||
         new_record.original_mime_type != cached_record.original_mime_type ||
         new_record.mime_type != cached_record.mime_type ||
         new_record.content_disposition != cached_record.content_disposition ||
         new_record.originating_host != cached_record.originating_host ||
         new_record.http_method != cached_record.http_method ||
         new_record.total_bytes != cached_record.total_bytes ||
         new_record.has_performed_background_download !=
             cached_record.has_performed_background_download;
}

void DownloadRecordService::InitializeDatabase(
    const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  base::FilePath downloads_dir =
      profile_path.Append(FILE_PATH_LITERAL("download_record_db"));
  base::FilePath db_path =
      downloads_dir.Append(FILE_PATH_LITERAL("DownloadRecord"));

  if (!base::CreateDirectory(downloads_dir)) {
    return;
  }

  auto database = std::make_unique<DownloadRecordDatabase>(db_path);
  sql::InitStatus init_status = database->Init();

  if (init_status == sql::INIT_OK) {
    database_ = std::move(database);
  }
}

void DownloadRecordService::LoadHistoricalRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return;
  }

  database_cache_.clear();

  std::vector<DownloadRecord> all_records = database_->GetAllDownloadRecords();
  for (const auto& record : all_records) {
    database_cache_[record.download_id] = record;
  }

  CleanupInconsistentStates();
}

void DownloadRecordService::CleanupInconsistentStates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return;
  }

  std::vector<std::string> records_to_fix;
  for (const auto& [id, record] : database_cache_) {
    if (record.state == web::DownloadTask::State::kInProgress ||
        record.state == web::DownloadTask::State::kNotStarted) {
      // Mark all unfinished records from previous session as failed.
      records_to_fix.push_back(id);
    }
  }

  if (records_to_fix.empty()) {
    return;
  }

  UpdateRecordsState(records_to_fix, web::DownloadTask::State::kFailed);
}

bool DownloadRecordService::InsertRecord(const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  if (database_->InsertDownloadRecord(record)) {
    database_cache_[record.download_id] = record;
    return true;
  }

  return false;
}

bool DownloadRecordService::UpdateRecord(const DownloadRecord& updated_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  auto it = database_cache_.find(updated_record.download_id);
  if (it == database_cache_.end()) {
    return false;
  }

  // Checks if we need to persist this update to database.
  bool needs_database_update = ShouldPersistUpdate(updated_record, it->second);

  if (!needs_database_update) {
    // No database update needed, just updates cache.
    database_cache_[updated_record.download_id] = updated_record;
    return true;
  }

  // Needs to update database.
  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  if (database_->UpdateDownloadRecord(updated_record)) {
    database_cache_[updated_record.download_id] = updated_record;
    return true;
  }

  return false;
}

bool DownloadRecordService::UpdateRecordsState(
    const std::vector<std::string>& download_ids,
    web::DownloadTask::State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (download_ids.empty()) {
    // Empty list is considered successful.
    return true;
  }

  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  if (database_->UpdateDownloadRecordsState(download_ids, new_state)) {
    // Updates cache for all successfully updated records.
    for (const std::string& download_id : download_ids) {
      auto it = database_cache_.find(download_id);
      if (it != database_cache_.end()) {
        it->second.state = new_state;
      }
    }
    return true;
  }

  return false;
}

bool DownloadRecordService::DeleteRecord(std::string_view id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  // Checks if record exists in cache.
  auto it = database_cache_.find(std::string(id));
  if (it == database_cache_.end()) {
    // Considers this a success since the record doesn't exist anyway.
    return true;
  }

  if (database_->DeleteDownloadRecord(std::string(id))) {
    database_cache_.erase(it);
    return true;
  }

  return false;
}

std::vector<DownloadRecord> DownloadRecordService::GetAllFromCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return {};
  }

  std::vector<DownloadRecord> records;
  records.reserve(database_cache_.size());

  for (const auto& [id, record] : database_cache_) {
    records.push_back(record);
  }

  return records;
}

std::optional<DownloadRecord> DownloadRecordService::GetByIdFromCache(
    std::string_view download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return std::nullopt;
  }

  auto it = database_cache_.find(std::string(download_id));
  if (it != database_cache_.end()) {
    return it->second;
  }

  return std::nullopt;
}
