// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service.h"

#import <algorithm>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer.h"

#pragma mark - Public

DownloadRecordService::DownloadRecordService() {
  CHECK(IsDownloadListEnabled());
}

DownloadRecordService::~DownloadRecordService() = default;

void DownloadRecordService::RecordDownload(web::DownloadTask* task) {
  if (!task) {
    return;
  }

  DownloadRecord record = DownloadRecord(task);
  std::string record_id = record.download_id;
  auto [iter, inserted] =
      downloads_.emplace(std::move(record_id), std::move(record));
  if (!inserted) {
    // Duplicate, do not notify.
    return;
  }

  NotifyDownloadAdded(iter->second);
  download_task_observations_.AddObservation(task);
}

std::vector<DownloadRecord> DownloadRecordService::GetAllDownloads() const {
  std::vector<DownloadRecord> records;
  records.reserve(downloads_.size());
  for (const auto& [id, record] : downloads_) {
    records.push_back(record);
  }
  return records;
}

void DownloadRecordService::AddObserver(DownloadRecordObserver* observer) {
  observers_.AddObserver(observer);
}

void DownloadRecordService::RemoveObserver(DownloadRecordObserver* observer) {
  observers_.RemoveObserver(observer);
}

#pragma mark - web::DownloadTaskObserver

void DownloadRecordService::OnDownloadUpdated(web::DownloadTask* task) {
  DownloadRecord* record = FindRecordByTask(task);
  if (!record) {
    return;
  }

  web::DownloadTask::State old_state = record->state;
  record->state = task->GetState();
  record->received_bytes = task->GetReceivedBytes();
  record->total_bytes = task->GetTotalBytes();
  record->progress_percent = task->GetPercentComplete();

  if (old_state != web::DownloadTask::State::kComplete &&
      record->state == web::DownloadTask::State::kComplete) {
    record->completed_time = base::Time::Now();
  }

  if (task->GetTotalBytes() > 0) {
    record->file_size = task->GetTotalBytes();
  }

  NotifyDownloadUpdated(record->download_id, record->state);
}

void DownloadRecordService::OnDownloadDestroyed(web::DownloadTask* task) {
  download_task_observations_.RemoveObservation(task);
}

#pragma mark - Private

void DownloadRecordService::NotifyDownloadAdded(const DownloadRecord& record) {
  observers_.Notify(&DownloadRecordObserver::OnDownloadAdded, record);
}

void DownloadRecordService::NotifyDownloadUpdated(
    std::string_view download_id,
    web::DownloadTask::State new_state) {
  observers_.Notify(&DownloadRecordObserver::OnDownloadUpdated, download_id,
                    new_state);
}

DownloadRecord* DownloadRecordService::FindRecordByTask(
    web::DownloadTask* task) {
  DCHECK(task);

  std::string task_id = base::SysNSStringToUTF8(task->GetIdentifier());
  auto it = downloads_.find(task_id);
  return (it != downloads_.end()) ? &it->second : nullptr;
}
