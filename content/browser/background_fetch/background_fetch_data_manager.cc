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
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/background_fetch/storage/mark_request_complete_task.h"
#include "content/browser/background_fetch/storage/match_requests_task.h"
#include "content/browser/background_fetch/storage/start_next_pending_request_task.h"
#include "content/browser/background_fetch/storage/update_registration_ui_task.h"
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
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      weak_ptr_factory_(this) {
  // Constructed on the UI thread, then used on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);
}

void BackgroundFetchDataManager::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The CacheStorageManager can only be accessed from the IO thread.
  cache_manager_ =
      base::WrapRefCounted(cache_storage_context_->cache_manager());

  // TODO(crbug.com/855199): Persist which registrations to cleanup on startup.
  Cleanup();

  DCHECK(cache_manager_);
}

void BackgroundFetchDataManager::AddObserver(
    BackgroundFetchDataManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.AddObserver(observer);
}

void BackgroundFetchDataManager::RemoveObserver(
    BackgroundFetchDataManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.RemoveObserver(observer);
}

void BackgroundFetchDataManager::Cleanup() {
  AddDatabaseTask(std::make_unique<background_fetch::CleanupTask>(this));
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::GetInitializationData(
    GetInitializationDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetInitializationDataTask>(
      this, std::move(callback)));
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    bool start_paused,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::CreateMetadataTask>(
      this, registration_id, requests, options, icon, start_paused,
      std::move(callback)));
}

void BackgroundFetchDataManager::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetRegistrationTask>(
      this, service_worker_registration_id, origin, developer_id,
      std::move(callback)));
}

void BackgroundFetchDataManager::UpdateRegistrationUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::UpdateRegistrationUITask>(
      this, registration_id, title, icon, std::move(callback)));
}

void BackgroundFetchDataManager::PopNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(
      std::make_unique<background_fetch::StartNextPendingRequestTask>(
          this, registration_id, std::move(callback)));
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    scoped_refptr<BackgroundFetchRequestInfo> request_info,
    MarkRequestCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::MarkRequestCompleteTask>(
      this, registration_id, std::move(request_info), std::move(callback)));
}

void BackgroundFetchDataManager::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::MatchRequestsTask>(
      this, registration_id, std::move(match_params), std::move(callback)));
}

void BackgroundFetchDataManager::MarkRegistrationForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool check_for_failure,
    MarkRegistrationForDeletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(
      std::make_unique<background_fetch::MarkRegistrationForDeletionTask>(
          this, registration_id, check_for_failure, std::move(callback)));
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::DeleteRegistrationTask>(
      this, registration_id.service_worker_registration_id(),
      registration_id.origin(), registration_id.unique_id(),
      std::move(callback)));
}

void BackgroundFetchDataManager::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetDeveloperIdsTask>(
      this, service_worker_registration_id, origin, std::move(callback)));
}

void BackgroundFetchDataManager::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Start();
}

void BackgroundFetchDataManager::OnTaskFinished(
    background_fetch::DatabaseTask* task) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!database_tasks_.empty());
  DCHECK_EQ(database_tasks_.front().get(), task);

  database_tasks_.pop();
  if (!database_tasks_.empty())
    database_tasks_.front()->Start();
}

BackgroundFetchDataManager* BackgroundFetchDataManager::data_manager() {
  return this;
}

}  // namespace content
