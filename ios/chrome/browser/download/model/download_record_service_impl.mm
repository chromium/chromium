// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_impl.h"

#import <memory>
#import <optional>
#import <string_view>
#import <vector>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_record_database.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#pragma mark - Public

DownloadRecordServiceImpl::DownloadRecordServiceImpl(
    const base::FilePath& profile_path)
    : database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  CHECK(IsDownloadListEnabled());
  CHECK(!profile_path.empty());

  // Detaches database sequence checker so it can bind to the DB thread later.
  DETACH_FROM_SEQUENCE(database_sequence_checker_);

  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<DownloadRecordServiceImpl> service,
                        const base::FilePath& profile_path) {
                       if (!service) {
                         return;
                       }
                       service->InitializeDatabase(profile_path);
                       service->LoadHistoricalRecords();
                     },
                     weak_ptr_factory_.GetWeakPtr(), profile_path));
}

DownloadRecordServiceImpl::~DownloadRecordServiceImpl() {
  // Ensure database is destroyed on the correct thread.
  if (database_) {
    database_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
  }
}

void DownloadRecordServiceImpl::RecordDownload(web::DownloadTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(task);

  DownloadRecord record = DownloadRecord(task);
  // Sets creation time when we first record the download.
  record.created_time = base::Time::Now();

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordServiceImpl::InsertRecord,
                     base::Unretained(this), record),
      base::BindOnce(
          [](base::WeakPtr<DownloadRecordServiceImpl> service,
             web::DownloadTask* task, const DownloadRecord& record,
             bool success) {
            if (service && success) {
              service->download_task_observations_.AddObservation(task);
              service->NotifyDownloadAdded(record);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), task, record));
}

void DownloadRecordServiceImpl::GetAllDownloadsAsync(
    DownloadRecordsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordServiceImpl::GetAllFromCache,
                     base::Unretained(this)),
      std::move(callback));
}

void DownloadRecordServiceImpl::GetDownloadByIdAsync(
    const std::string& download_id,
    DownloadRecordCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordServiceImpl::GetByIdFromCache,
                     base::Unretained(this), download_id),
      std::move(callback));
}

void DownloadRecordServiceImpl::RemoveDownloadByIdAsync(
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
      base::BindOnce(&DownloadRecordServiceImpl::DeleteRecord,
                     base::Unretained(this), download_id),
      base::BindOnce(
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

void DownloadRecordServiceImpl::UpdateDownloadFilePathAsync(
    const std::string& download_id,
    const base::FilePath& file_path,
    CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordServiceImpl::UpdateFilePathInRecord,
                     base::Unretained(this), download_id, file_path),
      base::BindOnce(
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
  // Search through all observed download tasks.
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

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadRecordServiceImpl::UpdateRecord,
                     base::Unretained(this), updated_record),
      base::BindOnce(
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

#pragma mark - Database Thread Operations

bool DownloadRecordServiceImpl::ShouldPersistUpdate(
    const DownloadRecord& new_record,
    const DownloadRecord& cached_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  // Incognito records are not persistently stored.
  if (cached_record.is_incognito) {
    return false;
  }

  // Persist only if critical fields have changed.
  // Progress fields (received_bytes, progress_percent) are not persisted to
  // database.
  return !new_record.EqualsExcludingProgress(cached_record);
}

void DownloadRecordServiceImpl::InitializeDatabase(
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

void DownloadRecordServiceImpl::LoadHistoricalRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return;
  }

  record_cache_.clear();

  std::vector<DownloadRecord> all_records = database_->GetAllDownloadRecords();
  for (const auto& record : all_records) {
    record_cache_[record.download_id] = record;
  }

  CleanupInconsistentStates();
}

void DownloadRecordServiceImpl::CleanupInconsistentStates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  if (!database_ || !database_->IsInitialized()) {
    return;
  }

  std::vector<std::string> records_to_fix;
  for (const auto& [id, record] : record_cache_) {
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

bool DownloadRecordServiceImpl::InsertRecord(const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  bool should_persist = !record.is_incognito;

  if (!should_persist) {
    record_cache_[record.download_id] = record;
    return true;
  }

  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  if (database_->InsertDownloadRecord(record)) {
    record_cache_[record.download_id] = record;
    return true;
  }

  return false;
}

std::optional<DownloadRecord> DownloadRecordServiceImpl::UpdateRecord(
    const DownloadRecord& updated_record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  auto existing_record_opt = GetByIdFromCache(updated_record.download_id);
  if (!existing_record_opt.has_value()) {
    return std::nullopt;
  }

  // Preserve created_time from existing record.
  DownloadRecord record_to_update = updated_record;
  record_to_update.created_time = existing_record_opt.value().created_time;

  // Determine if we need to persist this update to database.
  bool should_persist =
      ShouldPersistUpdate(record_to_update, existing_record_opt.value());

  if (!should_persist) {
    record_cache_[record_to_update.download_id] = record_to_update;
    return record_to_update;
  }

  if (!database_ || !database_->IsInitialized()) {
    return std::nullopt;
  }

  if (database_->UpdateDownloadRecord(record_to_update)) {
    record_cache_[record_to_update.download_id] = record_to_update;
    return record_to_update;
  }

  return std::nullopt;
}

bool DownloadRecordServiceImpl::UpdateRecordsState(
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
      auto it = record_cache_.find(download_id);
      if (it != record_cache_.end()) {
        it->second.state = new_state;
      }
    }
    return true;
  }

  return false;
}

std::optional<DownloadRecord> DownloadRecordServiceImpl::UpdateFilePathInRecord(
    const std::string& download_id,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  auto existing_record_opt = GetByIdFromCache(download_id);
  if (!existing_record_opt.has_value()) {
    return std::nullopt;
  }

  // Create updated record with new file path.
  DownloadRecord updated_record = existing_record_opt.value();
  updated_record.file_path = file_path;

  return UpdateRecord(updated_record);
}

bool DownloadRecordServiceImpl::DeleteRecord(std::string_view id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  // Check if record exists in cache.
  auto it = record_cache_.find(std::string(id));
  if (it == record_cache_.end()) {
    // Consider this a success since the record doesn't exist anyway.
    return true;
  }

  const DownloadRecord& record = it->second;
  bool should_persist = !record.is_incognito;

  if (!should_persist) {
    record_cache_.erase(it);
    return true;
  }

  if (!database_ || !database_->IsInitialized()) {
    return false;
  }

  if (database_->DeleteDownloadRecord(std::string(id))) {
    record_cache_.erase(it);
    return true;
  }

  return false;
}

std::vector<DownloadRecord> DownloadRecordServiceImpl::GetAllFromCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  std::vector<DownloadRecord> records;
  records.reserve(record_cache_.size());

  for (const auto& [id, record] : record_cache_) {
    records.push_back(record);
  }

  return records;
}

std::optional<DownloadRecord> DownloadRecordServiceImpl::GetByIdFromCache(
    std::string_view download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(database_sequence_checker_);

  auto it = record_cache_.find(std::string(download_id));
  if (it != record_cache_.end()) {
    return it->second;
  }

  return std::nullopt;
}
