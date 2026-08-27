// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

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
  CHECK(host_, base::NotFatalUntil::M158);
}

DatabaseTask::~DatabaseTask() = default;

base::WeakPtr<DatabaseTaskHost> DatabaseTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DatabaseTask::Finished() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M158);
  // Post the OnTaskFinished callback to the same thread, to allow the the
  // DatabaseTask to finish execution before deallocating it.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DatabaseTaskHost::OnTaskFinished,
                                host_->GetWeakPtr(), this));
}

void DatabaseTask::OnTaskFinished(DatabaseTask* finished_subtask) {
  size_t erased = active_subtasks_.erase(finished_subtask);
  CHECK_EQ(erased, 1u, base::NotFatalUntil::M158);
}

void DatabaseTask::AddDatabaseTask(std::unique_ptr<DatabaseTask> task) {
  CHECK_EQ(task->host_, data_manager(), base::NotFatalUntil::M158);
  data_manager()->AddDatabaseTask(std::move(task));
}

void DatabaseTask::AddSubTask(std::unique_ptr<DatabaseTask> task) {
  CHECK_EQ(task->host_, this, base::NotFatalUntil::M158);
  auto insert_result = active_subtasks_.emplace(task.get(), std::move(task));
  insert_result.first->second->Start();  // Start the subtask.
}

void DatabaseTask::AbandonFetches(int64_t service_worker_registration_id) {
  for (auto& observer : data_manager()->observers())
    observer.OnServiceWorkerDatabaseCorrupted(service_worker_registration_id);
}

void DatabaseTask::IsQuotaAvailable(const blink::StorageKey& storage_key,
                                    int64_t size,
                                    IsQuotaAvailableCallback callback) {
  CHECK(quota_manager_proxy(), base::NotFatalUntil::M158);
  CHECK_GT(size, 0, base::NotFatalUntil::M158);

  quota_manager_proxy()->GetUsageAndQuota(
      storage_key, base::SingleThreadTaskRunner::GetCurrentDefault(),
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

  CHECK_EQ(data.size(), 1u, base::NotFatalUntil::M158);
  int storage_version = proto::SV_UNINITIALIZED;

  if (!base::StringToInt(data[0], &storage_version) ||
      !proto::BackgroundFetchStorageVersion_IsValid(storage_version)) {
    storage_version = proto::SV_ERROR;
  }

  std::move(callback).Run(
      static_cast<proto::BackgroundFetchStorageVersion>(storage_version));
}

void DatabaseTask::SetStorageError(BackgroundFetchStorageError error) {
  CHECK_NE(BackgroundFetchStorageError::kNone, error,
           base::NotFatalUntil::M158);
  switch (storage_error_) {
    case BackgroundFetchStorageError::kNone:
      storage_error_ = error;
      break;
    case BackgroundFetchStorageError::kServiceWorkerStorageError:
    case BackgroundFetchStorageError::kCacheStorageError:
      CHECK(error == BackgroundFetchStorageError::kServiceWorkerStorageError ||
                error == BackgroundFetchStorageError::kCacheStorageError,
            base::NotFatalUntil::M158);
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

bool DatabaseTask::HasStorageError() {
  return storage_error_ != BackgroundFetchStorageError::kNone;
}

ServiceWorkerContextWrapper* DatabaseTask::service_worker_context() {
  CHECK(data_manager()->service_worker_context(), base::NotFatalUntil::M158);
  return data_manager()->service_worker_context();
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

const scoped_refptr<storage::QuotaManagerProxy>&
DatabaseTask::quota_manager_proxy() {
  return data_manager()->quota_manager_proxy();
}

void DatabaseTask::OpenCache(
    const BackgroundFetchRegistrationId& registration_id,
    int64_t trace_id,
    base::OnceCallback<void(blink::mojom::CacheStorageError)> callback) {
  CHECK(!cache_storage_cache_remote_.is_bound(), base::NotFatalUntil::M158);
  data_manager()->OpenCache(
      registration_id.storage_key(), registration_id.unique_id(), trace_id,
      base::BindOnce(&DatabaseTask::DidOpenCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DatabaseTask::DidOpenCache(
    base::OnceCallback<void(blink::mojom::CacheStorageError)> callback,
    blink::mojom::CacheStorage::OpenResult result) {
  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  cache_storage_cache_remote_.Bind(std::move(result.value()));
  std::move(callback).Run(blink::mojom::CacheStorageError::kSuccess);
}

void DatabaseTask::DeleteCache(
    const blink::StorageKey& storage_key,
    const std::string& unique_id,
    int64_t trace_id,
    blink::mojom::CacheStorage::DeleteCallback callback) {
  data_manager()->DeleteCache(storage_key, unique_id, trace_id,
                              std::move(callback));
}

}  // namespace background_fetch
}  // namespace content
