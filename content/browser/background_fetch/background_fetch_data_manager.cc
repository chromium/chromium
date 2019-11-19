// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/cleanup_task.h"
#include "content/browser/background_fetch/storage/create_metadata_task.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/background_fetch/storage/delete_registration_task.h"
#include "content/browser/background_fetch/storage/get_developer_ids_task.h"
#include "content/browser/background_fetch/storage/get_metadata_task.h"
#include "content/browser/background_fetch/storage/get_registration_task.h"
#include "content/browser/background_fetch/storage/get_request_blob_task.h"
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/background_fetch/storage/mark_request_complete_task.h"
#include "content/browser/background_fetch/storage/match_requests_task.h"
#include "content/browser/background_fetch/storage/start_next_pending_request_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<CacheStorageContextImpl> cache_storage_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : service_worker_context_(std::move(service_worker_context)),
      cache_storage_context_(std::move(cache_storage_context)),
      quota_manager_proxy_(std::move(quota_manager_proxy)) {
  // Constructed on the UI thread, then used on the service worker core thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);
}

void BackgroundFetchDataManager::InitializeOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  cache_manager_ = cache_storage_context_->CacheManager();

  // Delete inactive registrations still in the DB.
  Cleanup();

  DCHECK(cache_manager_);
}

void BackgroundFetchDataManager::AddObserver(
    BackgroundFetchDataManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  observers_.AddObserver(observer);
}

void BackgroundFetchDataManager::RemoveObserver(
    BackgroundFetchDataManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  observers_.RemoveObserver(observer);
}

void BackgroundFetchDataManager::Cleanup() {
  AddDatabaseTask(std::make_unique<background_fetch::CleanupTask>(this));
}

CacheStorageHandle BackgroundFetchDataManager::GetOrOpenCacheStorage(
    const url::Origin& origin,
    const std::string& unique_id) {
  auto it = cache_storage_handle_map_.find(unique_id);
  if (it != cache_storage_handle_map_.end()) {
    if (it->second.value()) {
      DCHECK_EQ(origin, it->second.value()->Origin());
    } else {
      // The backing CacheStorage has been forcibly closed due to an external
      // event. Re-open the CacheStorage and update the handle.
      it->second = cache_manager()->OpenCacheStorage(
          origin, CacheStorageOwner::kBackgroundFetch);
    }
    return it->second.Clone();
  }

  // This origin and unique_id has never been opened before. Open
  // the CacheStorage, remember the association in the map, and return the
  // handle.
  CacheStorageHandle handle = cache_manager()->OpenCacheStorage(
      origin, CacheStorageOwner::kBackgroundFetch);
  cache_storage_handle_map_.emplace(unique_id, handle.Clone());
  return handle;
}

void BackgroundFetchDataManager::ReleaseCacheStorage(
    const std::string& unique_id) {
  bool erased = cache_storage_handle_map_.erase(unique_id);
  DCHECK(erased);
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void BackgroundFetchDataManager::GetInitializationData(
    GetInitializationDataCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::GetInitializationDataTask>(
      this, std::move(callback)));
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    bool start_paused,
    CreateRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::CreateMetadataTask>(
      this, registration_id, std::move(requests), std::move(options), icon,
      start_paused, std::move(callback)));
}

void BackgroundFetchDataManager::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::GetRegistrationTask>(
      this, service_worker_registration_id, origin, developer_id,
      std::move(callback)));
}

void BackgroundFetchDataManager::PopNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(
      std::make_unique<background_fetch::StartNextPendingRequestTask>(
          this, registration_id, std::move(callback)));
}

void BackgroundFetchDataManager::GetRequestBlob(
    const BackgroundFetchRegistrationId& registration_id,
    const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
    GetRequestBlobCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::GetRequestBlobTask>(
      this, registration_id, request_info, std::move(callback)));
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    scoped_refptr<BackgroundFetchRequestInfo> request_info,
    MarkRequestCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::MarkRequestCompleteTask>(
      this, registration_id, std::move(request_info), std::move(callback)));
}

void BackgroundFetchDataManager::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::MatchRequestsTask>(
      this, registration_id, std::move(match_params), std::move(callback)));
}

void BackgroundFetchDataManager::MarkRegistrationForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool check_for_failure,
    MarkRegistrationForDeletionCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(
      std::make_unique<background_fetch::MarkRegistrationForDeletionTask>(
          this, registration_id, check_for_failure, std::move(callback)));
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::DeleteRegistrationTask>(
      this, registration_id.service_worker_registration_id(),
      registration_id.origin(), registration_id.unique_id(),
      std::move(callback)));
}

void BackgroundFetchDataManager::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  AddDatabaseTask(std::make_unique<background_fetch::GetDeveloperIdsTask>(
      this, service_worker_registration_id, origin, std::move(callback)));
}

void BackgroundFetchDataManager::ShutdownOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Release reference to CacheStorageManager. DatabaseTasks that need it
  // hold their own copy, so they can continue their work.
  cache_manager_ = nullptr;

  shutting_down_ = true;
}

void BackgroundFetchDataManager::AddDatabaseTask(
    std::unique_ptr<background_fetch::DatabaseTask> task) {
  // If Shutdown was called don't add any new tasks.
  if (shutting_down_)
    return;

  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1u)
    database_tasks_.front()->Start();
}

void BackgroundFetchDataManager::OnTaskFinished(
    background_fetch::DatabaseTask* task) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  DCHECK(!database_tasks_.empty());
  DCHECK_EQ(database_tasks_.front().get(), task);

  database_tasks_.pop();
  if (!database_tasks_.empty())
    database_tasks_.front()->Start();
}

BackgroundFetchDataManager* BackgroundFetchDataManager::data_manager() {
  return this;
}

base::WeakPtr<background_fetch::DatabaseTaskHost>
BackgroundFetchDataManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
