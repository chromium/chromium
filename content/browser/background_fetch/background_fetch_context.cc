// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_registration_service_impl.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

using FailureReason = blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<CacheStorageContextImpl>& cache_storage_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context)
    : base::RefCountedDeleteOnSequence<BackgroundFetchContext>(
          base::CreateSequencedTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()})),
      browser_context_(browser_context),
      service_worker_context_(service_worker_context),
      devtools_context_(std::move(devtools_context)),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(browser_context_) {
  // Although this lives only on the service worker core thread, it is
  // constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_worker_context_);

  data_manager_ = std::make_unique<BackgroundFetchDataManager>(
      browser_context_, service_worker_context, cache_storage_context,
      std::move(quota_manager_proxy));
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      this, data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      devtools_context_.get(), service_worker_context_);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  service_worker_context_->RemoveObserver(scheduler_.get());
  data_manager_->RemoveObserver(scheduler_.get());
}

void BackgroundFetchContext::InitializeOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  service_worker_context_->AddObserver(scheduler_.get());

  data_manager_->AddObserver(scheduler_.get());
  data_manager_->InitializeOnCoreThread();
  data_manager_->GetInitializationData(
      base::BindOnce(&BackgroundFetchContext::DidGetInitializationData,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundFetchContext::DidGetInitializationData(
    blink::mojom::BackgroundFetchError error,
    std::vector<background_fetch::BackgroundFetchInitializationData>
        initialization_data) {
  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  background_fetch::RecordRegistrationsOnStartup(initialization_data.size());

  for (auto& data : initialization_data) {
    for (auto& observer : data_manager_->observers()) {
      observer.OnRegistrationLoadedAtStartup(
          data.registration_id, *data.registration_data, data.options.Clone(),
          data.icon, data.num_completed_requests, data.num_requests,
          data.active_fetch_requests);
    }
  }
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  data_manager_->GetRegistration(
      service_worker_registration_id, origin, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  data_manager_->GetDeveloperIdsForServiceWorker(service_worker_registration_id,
                                                 origin, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    BackgroundFetchRegistrationId registration_id,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(
        error, nullptr /* blink::mojom::BackgroundFetchRegistration */);
    return;
  }

  for (auto& observer : data_manager_->observers())
    observer.OnRegistrationQueried(registration_id, registration_data.get());

  auto registration = blink::mojom::BackgroundFetchRegistration::New(
      std::move(registration_data),
      BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
          std::move(registration_id), this));
  std::move(callback).Run(error, std::move(registration));
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int render_frame_tree_node_id,
    const WebContents::Getter& wc_getter,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);

  delegate_proxy_.GetPermissionForOrigin(
      registration_id.origin(), wc_getter,
      base::BindOnce(&BackgroundFetchContext::DidGetPermission,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(requests), std::move(options), icon,
                     std::move(ukm_data), render_frame_tree_node_id));
}

void BackgroundFetchContext::DidGetPermission(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int frame_tree_node_id,
    BackgroundFetchPermission permission) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&background_fetch::RecordBackgroundFetchUkmEvent,
                     registration_id.origin(), requests.size(), options.Clone(),
                     icon, std::move(ukm_data), frame_tree_node_id,
                     permission));

  if (permission != BackgroundFetchPermission::BLOCKED) {
    data_manager_->CreateRegistration(
        registration_id, std::move(requests), std::move(options), icon,
        /* start_paused= */ permission == BackgroundFetchPermission::ASK,
        base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                       weak_factory_.GetWeakPtr(), registration_id));
    return;
  }

  // No permission, the fetch should be rejected.
  std::move(fetch_callbacks_[registration_id])
      .Run(blink::mojom::BackgroundFetchError::PERMISSION_DENIED, nullptr);
  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  auto iter = fetch_callbacks_.find(registration_id);

  // The fetch might have been abandoned already if the Service Worker was
  // unregistered or corrupted while registration was in progress.
  if (iter == fetch_callbacks_.end())
    return;

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    auto registration = blink::mojom::BackgroundFetchRegistration::New(
        std::move(registration_data),
        BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
            registration_id, this));
    std::move(iter->second).Run(error, std::move(registration));
  } else {
    std::move(iter->second).Run(error, /* registration= */ nullptr);
  }

  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    mojo::PendingRemote<blink::mojom::BackgroundFetchRegistrationObserver>
        observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchRegistrationService::UpdateUICallback
        callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  delegate_proxy_.UpdateUI(registration_id.unique_id(), title, icon,
                           std::move(callback));
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationService::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  scheduler_->Abort(registration_id, FailureReason::CANCELLED_BY_DEVELOPER,
                    std::move(callback));
}

void BackgroundFetchContext::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    blink::mojom::BackgroundFetchRegistrationService::MatchRequestsCallback
        callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  data_manager_->MatchRequests(
      registration_id, std::move(match_params),
      base::BindOnce(&BackgroundFetchContext::DidGetMatchingRequests,
                     weak_factory_.GetWeakPtr(), registration_id.unique_id(),
                     std::move(callback)));
}

void BackgroundFetchContext::DidGetMatchingRequests(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationService::MatchRequestsCallback
        callback,
    blink::mojom::BackgroundFetchError error,
    std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> settled_fetches) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (error != blink::mojom::BackgroundFetchError::NONE)
    DCHECK(settled_fetches.empty());

  // TODO(crbug.com/850512): We don't need to call this for requests that're
  // complete.
  // AddObservedUrl() is a no-op in those cases, but we can skip calling it.
  for (const auto& fetch : settled_fetches)
    registration_notifier_->AddObservedUrl(unique_id, fetch->request->url);

  std::move(callback).Run(std::move(settled_fetches));
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchContext::ShutdownOnCoreThread, this));
}

void BackgroundFetchContext::ShutdownOnCoreThread() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  data_manager_->ShutdownOnCoreThread();
}

void BackgroundFetchContext::SetDataManagerForTesting(
    std::unique_ptr<BackgroundFetchDataManager> data_manager) {
  DCHECK(data_manager);
  data_manager_ = std::move(data_manager);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      this, data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      devtools_context_.get(), service_worker_context_);
}

}  // namespace content
