// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_registration_service_impl.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

using FailureReason = blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchContext::BackgroundFetchContext(
    base::WeakPtr<StoragePartitionImpl> storage_partition,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    DevToolsBackgroundServicesContextImpl& devtools_context)
    : service_worker_context_(service_worker_context),
      devtools_context_(&devtools_context),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_worker_context_);

  data_manager_ = std::make_unique<BackgroundFetchDataManager>(
      storage_partition, service_worker_context,
      std::move(quota_manager_proxy));
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      this, data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      *devtools_context_, service_worker_context_);
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->RemoveObserver(scheduler_.get());
  data_manager_->RemoveObserver(scheduler_.get());
}

void BackgroundFetchContext::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_context_->AddObserver(scheduler_.get());

  data_manager_->AddObserver(scheduler_.get());
  data_manager_->Initialize();
  data_manager_->GetInitializationData(
      base::BindOnce(&BackgroundFetchContext::DidGetInitializationData,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundFetchContext::DidGetInitializationData(
    blink::mojom::BackgroundFetchError error,
    std::vector<background_fetch::BackgroundFetchInitializationData>
        initialization_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  for (auto& data : initialization_data) {
    for (auto& observer : data_manager_->observers()) {
      observer.OnRegistrationLoadedAtStartup(
          data.registration_id, *data.registration_data, data.options.Clone(),
          data.icon, data.num_completed_requests, data.num_requests,
          data.active_fetch_requests, data.isolation_info);
    }
  }
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_manager_->GetRegistration(
      service_worker_registration_id, storage_key, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const blink::StorageKey& storage_key,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_manager_->GetDeveloperIdsForServiceWorker(
      service_worker_registration_id, storage_key, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    BackgroundFetchRegistrationId registration_id,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
          std::move(registration_id), weak_factory_.GetWeakPtr()));
  std::move(callback).Run(error, std::move(registration));
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    RenderProcessHost* rph,
    RenderFrameHostImpl* rfh,
    const net::IsolationInfo& isolation_info,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);

  auto rfh_id = rfh ? rfh->GetGlobalId() : GlobalRenderFrameHostId();
  delegate_proxy_.GetPermissionForOrigin(
      registration_id.storage_key().origin(), rph, rfh,
      base::BindOnce(&BackgroundFetchContext::DidGetPermission,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(requests), std::move(options), icon,
                     std::move(ukm_data), rfh_id, isolation_info));
}

void BackgroundFetchContext::DidGetPermission(
    const BackgroundFetchRegistrationId& registration_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    const GlobalRenderFrameHostId& rfh_id,
    const net::IsolationInfo& isolation_info,
    BackgroundFetchPermission permission) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  background_fetch::RecordBackgroundFetchUkmEvent(
      registration_id.storage_key(), requests.size(), options.Clone(), icon,
      std::move(ukm_data), RenderFrameHostImpl::FromID(rfh_id), permission);

  if (permission != BackgroundFetchPermission::BLOCKED) {
    data_manager_->CreateRegistration(
        registration_id, std::move(requests), std::move(options), icon,
        /* start_paused= */ permission == BackgroundFetchPermission::ASK,
        isolation_info,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = fetch_callbacks_.find(registration_id);

  // The fetch might have been abandoned already if the Service Worker was
  // unregistered or corrupted while registration was in progress.
  if (iter == fetch_callbacks_.end())
    return;

  if (error == blink::mojom::BackgroundFetchError::NONE) {
    auto registration = blink::mojom::BackgroundFetchRegistration::New(
        std::move(registration_data),
        BackgroundFetchRegistrationServiceImpl::CreateInterfaceInfo(
            registration_id, weak_factory_.GetWeakPtr()));
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const std::optional<std::string>& title,
    const std::optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchRegistrationService::UpdateUICallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delegate_proxy_.UpdateUI(registration_id.unique_id(), title, icon,
                           std::move(callback));
}

base::WeakPtr<BackgroundFetchContext> BackgroundFetchContext::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationService::AbortCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scheduler_->Abort(registration_id, FailureReason::CANCELLED_BY_DEVELOPER,
                    std::move(callback));
}

void BackgroundFetchContext::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    blink::mojom::BackgroundFetchRegistrationService::MatchRequestsCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != blink::mojom::BackgroundFetchError::NONE)
    DCHECK(settled_fetches.empty());

  // TODO(crbug.com/40579759): We don't need to call this for requests that're
  // complete.
  // AddObservedUrl() is a no-op in those cases, but we can skip calling it.
  for (const auto& fetch : settled_fetches)
    registration_notifier_->AddObservedUrl(unique_id, fetch->request->url);

  std::move(callback).Run(std::move(settled_fetches));
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_manager_->Shutdown();
  scheduler_->Shutdown();
  devtools_context_ = nullptr;
}

void BackgroundFetchContext::SetDataManagerForTesting(
    std::unique_ptr<BackgroundFetchDataManager> data_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_manager);
  CHECK(devtools_context_);
  data_manager_ = std::move(data_manager);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(
      this, data_manager_.get(), registration_notifier_.get(), &delegate_proxy_,
      *devtools_context_, service_worker_context_);
}

}  // namespace content
