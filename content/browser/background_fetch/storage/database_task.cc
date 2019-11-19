// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {
namespace background_fetch {

namespace {

void DidGetUsageAndQuota(DatabaseTask::IsQuotaAvailableCallback callback,
                         int64_t size,
                         blink::mojom::QuotaStatusCode status,
                         int64_t usage,
                         int64_t quota) {
  bool is_available =
      status == blink::mojom::QuotaStatusCode::kOk && (usage + size) <= quota;

  std::move(callback).Run(is_available);
}

}  // namespace

DatabaseTaskHost::DatabaseTaskHost() = default;

DatabaseTaskHost::~DatabaseTaskHost() = default;

DatabaseTask::DatabaseTask(DatabaseTaskHost* host) : host_(host) {
  DCHECK(host_);
  // Hold a reference to the CacheStorageManager.
  cache_manager_ = data_manager()->cache_manager();
}

DatabaseTask::~DatabaseTask() = default;

base::WeakPtr<DatabaseTaskHost> DatabaseTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DatabaseTask::Finished() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  // Post the OnTaskFinished callback to the same thread, to allow the the
  // DatabaseTask to finish execution before deallocating it.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DatabaseTaskHost::OnTaskFinished,
                                host_->GetWeakPtr(), this));
}

void DatabaseTask::OnTaskFinished(DatabaseTask* finished_subtask) {
  size_t erased = active_subtasks_.erase(finished_subtask);
  DCHECK_EQ(erased, 1u);
}

void DatabaseTask::AddDatabaseTask(std::unique_ptr<DatabaseTask> task) {
  DCHECK_EQ(task->host_, data_manager());
  data_manager()->AddDatabaseTask(std::move(task));
}

void DatabaseTask::AddSubTask(std::unique_ptr<DatabaseTask> task) {
  DCHECK_EQ(task->host_, this);
  auto insert_result = active_subtasks_.emplace(task.get(), std::move(task));
  insert_result.first->second->Start();  // Start the subtask.
}

void DatabaseTask::AbandonFetches(int64_t service_worker_registration_id) {
  for (auto& observer : data_manager()->observers())
    observer.OnServiceWorkerDatabaseCorrupted(service_worker_registration_id);
}

void DatabaseTask::IsQuotaAvailable(const url::Origin& origin,
                                    int64_t size,
                                    IsQuotaAvailableCallback callback) {
  DCHECK(quota_manager_proxy());
  DCHECK_GT(size, 0);
  quota_manager_proxy()->GetUsageAndQuota(
      base::ThreadTaskRunnerHandle::Get().get(), origin,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&DidGetUsageAndQuota, std::move(callback), size));
}

void DatabaseTask::GetStorageVersion(int64_t service_worker_registration_id,
                                     const std::string& unique_id,
                                     StorageVersionCallback callback) {
  service_worker_context()->GetRegistrationUserData(
      service_worker_registration_id, {StorageVersionKey(unique_id)},
      base::BindOnce(&DatabaseTask::DidGetStorageVersion,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DatabaseTask::DidGetStorageVersion(StorageVersionCallback callback,
                                        const std::vector<std::string>& data,
                                        blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
      std::move(callback).Run(proto::SV_UNINITIALIZED);
      return;
    case DatabaseStatus::kFailed:
      std::move(callback).Run(proto::SV_ERROR);
      return;
    case DatabaseStatus::kOk:
      break;
  }

  DCHECK_EQ(data.size(), 1u);
  int storage_version = proto::SV_UNINITIALIZED;

  if (!base::StringToInt(data[0], &storage_version) ||
      !proto::BackgroundFetchStorageVersion_IsValid(storage_version)) {
    storage_version = proto::SV_ERROR;
  }

  std::move(callback).Run(
      static_cast<proto::BackgroundFetchStorageVersion>(storage_version));
}

void DatabaseTask::SetStorageError(BackgroundFetchStorageError error) {
  DCHECK_NE(BackgroundFetchStorageError::kNone, error);
  switch (storage_error_) {
    case BackgroundFetchStorageError::kNone:
      storage_error_ = error;
      break;
    case BackgroundFetchStorageError::kServiceWorkerStorageError:
    case BackgroundFetchStorageError::kCacheStorageError:
      DCHECK(error == BackgroundFetchStorageError::kServiceWorkerStorageError ||
             error == BackgroundFetchStorageError::kCacheStorageError);
      if (storage_error_ != error)
        storage_error_ = BackgroundFetchStorageError::kStorageError;
      break;
    case BackgroundFetchStorageError::kStorageError:
      break;
  }
}

void DatabaseTask::SetStorageErrorAndFinish(BackgroundFetchStorageError error) {
  SetStorageError(error);
  FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
}

void DatabaseTask::ReportStorageError() {
  if (host_ != data_manager())
    return;  // This is a SubTask.

  base::UmaHistogramEnumeration("BackgroundFetch.Storage." + HistogramName(),
                                storage_error_);
}

bool DatabaseTask::HasStorageError() {
  return storage_error_ != BackgroundFetchStorageError::kNone;
}

std::string DatabaseTask::HistogramName() const {
  NOTREACHED() << "HistogramName needs to be provided.";
  return "GeneralDatabaseTask";
}

ServiceWorkerContextWrapper* DatabaseTask::service_worker_context() {
  DCHECK(data_manager()->service_worker_context());
  return data_manager()->service_worker_context();
}

CacheStorageManager* DatabaseTask::cache_manager() {
  DCHECK(cache_manager_);
  return cache_manager_.get();
}

std::set<std::string>& DatabaseTask::ref_counted_unique_ids() {
  return data_manager()->ref_counted_unique_ids();
}

ChromeBlobStorageContext* DatabaseTask::blob_storage_context() {
  return data_manager()->blob_storage_context();
}

BackgroundFetchDataManager* DatabaseTask::data_manager() {
  return host_->data_manager();
}

storage::QuotaManagerProxy* DatabaseTask::quota_manager_proxy() {
  return data_manager()->quota_manager_proxy();
}

CacheStorageHandle DatabaseTask::GetOrOpenCacheStorage(
    const BackgroundFetchRegistrationId& registration_id) {
  return GetOrOpenCacheStorage(registration_id.origin(),
                               registration_id.unique_id());
}

CacheStorageHandle DatabaseTask::GetOrOpenCacheStorage(
    const url::Origin& origin,
    const std::string& unique_id) {
  return data_manager()->GetOrOpenCacheStorage(origin, unique_id);
}

void DatabaseTask::ReleaseCacheStorage(const std::string& unique_id) {
  data_manager()->ReleaseCacheStorage(unique_id);
}

}  // namespace background_fetch
}  // namespace content
