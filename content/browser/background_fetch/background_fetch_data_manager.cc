// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
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
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : service_worker_context_(std::move(service_worker_context)),
      storage_partition_(storage_partition),
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

  // Delete inactive registrations still in the DB.
  Cleanup();
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

mojo::Remote<blink::mojom::CacheStorage>&
BackgroundFetchDataManager::GetOrOpenCacheStorage(
    const url::Origin& origin,
    const std::string& unique_id) {
  auto it = cache_storage_remote_map_.find(unique_id);
  if (it != cache_storage_remote_map_.end()) {
    // TODO(enne): should we store the origin so we can DCHECK it matches here?
    return it->second;
  }

  // This origin and unique_id has never been opened before.
  mojo::Remote<blink::mojom::CacheStorage> remote;
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
  storage_partition_->GetCacheStorageControl()->AddReceiver(
      cross_origin_embedder_policy, mojo::NullRemote(), origin,
      storage::mojom::CacheStorageOwner::kBackgroundFetch,
      remote.BindNewPipeAndPassReceiver());

  auto result = cache_storage_remote_map_.emplace(unique_id, std::move(remote));
  DCHECK(result.second);
  return result.first->second;
}

void BackgroundFetchDataManager::OpenCache(
    const url::Origin& origin,
    const std::string& unique_id,
    int64_t trace_id,
    blink::mojom::CacheStorage::OpenCallback callback) {
  auto& cache_storage = GetOrOpenCacheStorage(origin, unique_id);
  cache_storage->Open(base::UTF8ToUTF16(unique_id), trace_id,
                      std::move(callback));
}

void BackgroundFetchDataManager::DeleteCache(
    const url::Origin& origin,
    const std::string& unique_id,
    int64_t trace_id,
    blink::mojom::CacheStorage::DeleteCallback callback) {
  auto& cache_storage = GetOrOpenCacheStorage(origin, unique_id);
  cache_storage->Delete(
      base::UTF8ToUTF16(unique_id), trace_id,
      base::BindOnce(&BackgroundFetchDataManager::DidDeleteCache,
                     weak_ptr_factory_.GetWeakPtr(), unique_id,
                     std::move(callback)));
}

void BackgroundFetchDataManager::DidDeleteCache(
    const std::string& unique_id,
    blink::mojom::CacheStorage::DeleteCallback callback,
    blink::mojom::CacheStorageError result) {
  // Preserve the lifetime of the cache storage remote until here so that this
  // DidDeleteCache callback will not be dropped.
  cache_storage_remote_map_.erase(unique_id);
  std::move(callback).Run(result);
}

void BackgroundFetchDataManager::HasCache(
    const url::Origin& origin,
    const std::string& unique_id,
    int64_t trace_id,
    blink::mojom::CacheStorage::HasCallback callback) {
  auto& cache_storage = GetOrOpenCacheStorage(origin, unique_id);
  cache_storage->Has(base::UTF8ToUTF16(unique_id), trace_id,
                     std::move(callback));
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
