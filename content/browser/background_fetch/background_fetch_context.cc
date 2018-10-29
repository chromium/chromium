// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_context.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

using FailureReason = blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchContext::BackgroundFetchContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<content::CacheStorageContextImpl>&
        cache_storage_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : browser_context_(browser_context),
      service_worker_context_(service_worker_context),
      event_dispatcher_(service_worker_context),
      registration_notifier_(
          std::make_unique<BackgroundFetchRegistrationNotifier>()),
      delegate_proxy_(browser_context_->GetBackgroundFetchDelegate()),
      weak_factory_(this) {
  // Although this lives only on the IO thread, it is constructed on UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(service_worker_context_);

  data_manager_ = std::make_unique<BackgroundFetchDataManager>(
      browser_context_, service_worker_context, cache_storage_context,
      std::move(quota_manager_proxy));
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(data_manager_.get());
  delegate_proxy_.SetClickEventDispatcher(base::BindRepeating(
      &BackgroundFetchContext::DispatchClickEvent, weak_factory_.GetWeakPtr()));
}

BackgroundFetchContext::~BackgroundFetchContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(this);
  data_manager_->RemoveObserver(this);
}

void BackgroundFetchContext::InitializeOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->AddObserver(this);

  data_manager_->AddObserver(this);
  data_manager_->InitializeOnIOThread();
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
    CreateController(data.registration_id, data.registration, data.options,
                     data.icon, data.ui_title, data.num_completed_requests,
                     data.num_requests, std::move(data.active_fetch_requests),
                     /* start_paused = */ false);
  }
}

void BackgroundFetchContext::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->GetRegistration(
      service_worker_registration_id, origin, developer_id,
      base::BindOnce(&BackgroundFetchContext::DidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  data_manager_->GetDeveloperIdsForServiceWorker(service_worker_registration_id,
                                                 origin, std::move(callback));
}

void BackgroundFetchContext::DidGetRegistration(
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::BackgroundFetchError::NONE) {
    std::move(callback).Run(error,
                            base::nullopt /* BackgroundFetchRegistration */);
    return;
  }

  BackgroundFetchRegistration updated_registration(registration);

  // The data manager only has the number of bytes from completed downloads, so
  // augment this with the number of downloaded bytes from in-progress jobs.
  DCHECK(job_controllers_.count(registration.unique_id));
  updated_registration.downloaded +=
      job_controllers_[registration.unique_id]->GetInProgressDownloadedBytes();

  std::move(callback).Run(error, updated_registration);
}

void BackgroundFetchContext::StartFetch(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    RenderFrameHost* render_frame_host,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |registration_id| should be unique even if developer id has been
  // duplicated, because the caller of this function generates a new unique_id
  // every time, which is what BackgroundFetchRegistrationId's comparison
  // operator uses.
  DCHECK_EQ(0u, fetch_callbacks_.count(registration_id));
  fetch_callbacks_[registration_id] = std::move(callback);
  int frame_tree_node_id =
      render_frame_host ? render_frame_host->GetFrameTreeNodeId() : 0;

  GetPermissionForOrigin(
      registration_id.origin(), render_frame_host,
      base::BindOnce(&BackgroundFetchContext::DidGetPermission,
                     weak_factory_.GetWeakPtr(), registration_id, requests,
                     options, icon, std::move(ukm_data), frame_tree_node_id));
}

void BackgroundFetchContext::GetPermissionForOrigin(
    const url::Origin& origin,
    RenderFrameHost* render_frame_host,
    GetPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceRequestInfo::WebContentsGetter wc_getter = base::NullCallback();

  // Permissions need to go through the DownloadRequestLimiter if the fetch
  // is started from a top-level frame.
  if (render_frame_host && !render_frame_host->GetParent()) {
    wc_getter = base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                                    render_frame_host->GetFrameTreeNodeId());
  }

  delegate_proxy_.GetPermissionForOrigin(origin, std::move(wc_getter),
                                         std::move(callback));
}

void BackgroundFetchContext::DidGetPermission(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    int frame_tree_node_id,
    BackgroundFetchPermission permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&background_fetch::RecordBackgroundFetchUkmEvent,
                     registration_id.origin(), requests, options, icon,
                     std::move(ukm_data), frame_tree_node_id, permission));

  if (permission != BackgroundFetchPermission::BLOCKED) {
    // TODO(crbug.com/886896): Passed paused flag to CreateRegistration.
    data_manager_->BackgroundFetchDataManager::CreateRegistration(
        registration_id, requests, options, icon,
        permission == BackgroundFetchPermission::ASK /* start_paused */,
        base::BindOnce(&BackgroundFetchContext::DidCreateRegistration,
                       weak_factory_.GetWeakPtr(), registration_id));
    return;
  }

  // No permission, the fetch should be rejected.
  background_fetch::RecordRegistrationCreatedError(
      blink::mojom::BackgroundFetchError::PERMISSION_DENIED);
  std::move(fetch_callbacks_[registration_id])
      .Run(blink::mojom::BackgroundFetchError::PERMISSION_DENIED,
           base::nullopt);
}

void BackgroundFetchContext::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  delegate_proxy_.GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchContext::DidCreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchError error,
    const BackgroundFetchRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  background_fetch::RecordRegistrationCreatedError(error);

  auto iter = fetch_callbacks_.find(registration_id);

  // The fetch might have been abandoned already if the Service Worker was
  // unregistered or corrupted while registration was in progress.
  if (iter == fetch_callbacks_.end())
    return;

  if (error == blink::mojom::BackgroundFetchError::NONE)
    std::move(iter->second).Run(error, registration);
  else
    std::move(iter->second).Run(error, base::nullopt /* registration */);

  fetch_callbacks_.erase(registration_id);
}

void BackgroundFetchContext::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  registration_notifier_->AddObserver(unique_id, std::move(observer));
}

void BackgroundFetchContext::UpdateUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The registration must a) still be active, or b) have completed/failed (not
  // aborted) with the waitUntil promise from that event not yet resolved.
  if (!job_controllers_.count(registration_id.unique_id())) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  data_manager_->UpdateRegistrationUI(registration_id, title, icon,
                                      std::move(callback));
}

void BackgroundFetchContext::OnServiceWorkerDatabaseCorrupted(
    int64_t service_worker_registration_id) {
  AbandonFetches(service_worker_registration_id);
}

void BackgroundFetchContext::OnQuotaExceeded(
    const BackgroundFetchRegistrationId& registration_id) {
  auto job_it = job_controllers_.find(registration_id.unique_id());
  if (job_it != job_controllers_.end() && job_it->second)
    job_it->second->Abort(FailureReason::QUOTA_EXCEEDED);
}

void BackgroundFetchContext::AbandonFetches(
    int64_t service_worker_registration_id) {
  // Abandon all active fetches associated with this service worker.
  // BackgroundFetchJobController::Abort() will eventually lead to deletion of
  // the controller from job_controllers, hence we can't use a range based
  // for-loop here.
  for (auto iter = job_controllers_.begin(); iter != job_controllers_.end();
       /* no_increment */) {
    auto saved_iter = iter;
    iter++;
    if (service_worker_registration_id ==
            blink::mojom::kInvalidServiceWorkerRegistrationId ||
        saved_iter->second->registration_id()
                .service_worker_registration_id() ==
            service_worker_registration_id) {
      DCHECK(saved_iter->second);

      saved_iter->second->Abort(FailureReason::SERVICE_WORKER_UNAVAILABLE);
    }
  }

  for (auto iter = fetch_callbacks_.begin(); iter != fetch_callbacks_.end();
       /* no increment */) {
    if (service_worker_registration_id ==
            blink::mojom::kInvalidServiceWorkerRegistrationId ||
        iter->first.service_worker_registration_id() ==
            service_worker_registration_id) {
      DCHECK(iter->second);
      std::move(iter->second)
          .Run(blink::mojom::BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE,
               base::nullopt /* BackgroundFetchRegistration */);
      iter = fetch_callbacks_.erase(iter);
    } else
      iter++;
  }
}

void BackgroundFetchContext::OnRegistrationCreated(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRegistration& registration,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    int num_requests,
    bool start_paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (hang_registration_creation_for_testing_) {
    // Hang here, to allow time for testing races. For instance, this helps us
    // test the behavior when a service worker gets unregistered before the
    // controller can be created.
    return;
  }

  // TODO(peter): When this moves to the BackgroundFetchScheduler, only create
  // a controller when the background fetch can actually be started.

  CreateController(registration_id, registration, options, icon, options.title,
                   0u /* num_completed_requests */, num_requests,
                   {} /* active_fetch_requests */, start_paused);
}

void BackgroundFetchContext::OnUpdatedUI(
    const BackgroundFetchRegistrationId& registration_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = job_controllers_.find(registration_id.unique_id());
  if (iter != job_controllers_.end())
    iter->second->UpdateUI(title, icon);
}

void BackgroundFetchContext::OnRegistrationDeleted(
    int64_t service_worker_registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbandonFetches(service_worker_registration_id);
}

void BackgroundFetchContext::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  AbandonFetches(blink::mojom::kInvalidServiceWorkerRegistrationId);
}

void BackgroundFetchContext::OnFetchStorageError(
    const BackgroundFetchRegistrationId& registration_id) {
  auto controllers_iter = job_controllers_.find(registration_id.unique_id());
  if (controllers_iter == job_controllers_.end())
    return;

  controllers_iter->second->Abort(FailureReason::SERVICE_WORKER_UNAVAILABLE);
}

void BackgroundFetchContext::CreateController(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRegistration& registration,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    const std::string& ui_title,
    size_t num_completed_requests,
    size_t num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    bool start_paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controller = std::make_unique<BackgroundFetchJobController>(
      &delegate_proxy_, scheduler_.get(), registration_id, options, icon,
      registration.downloaded,
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_.get())),
      base::BindOnce(
          &BackgroundFetchContext::DidFinishJob, weak_factory_.GetWeakPtr(),
          base::Bind(&background_fetch::RecordSchedulerFinishedError)));

  controller->InitializeRequestStatus(num_completed_requests, num_requests,
                                      std::move(active_fetch_requests),
                                      ui_title, start_paused);
  scheduler_->AddJobController(controller.get());
  job_controllers_.emplace(registration_id.unique_id(), std::move(controller));
}

void BackgroundFetchContext::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchService::AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto controllers_iter = job_controllers_.find(registration_id.unique_id());
  if (controllers_iter == job_controllers_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  controllers_iter->second->Abort(FailureReason::CANCELLED_BY_DEVELOPER);

  DidFinishJob(std::move(callback), registration_id,
               FailureReason::CANCELLED_BY_DEVELOPER);
}

void BackgroundFetchContext::DidFinishJob(
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    const BackgroundFetchRegistrationId& registration_id,
    FailureReason failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If the registration was aborted, this will also propagate the event to any
  // active JobController, to terminate in-progress requests.
  data_manager_->MarkRegistrationForDeletion(
      registration_id,
      /* check_for_failure= */ failure_reason == FailureReason::NONE,
      base::BindOnce(&BackgroundFetchContext::DidMarkForDeletion,
                     weak_factory_.GetWeakPtr(), registration_id,
                     std::move(callback)));
}

void BackgroundFetchContext::DidMarkForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    base::OnceCallback<void(blink::mojom::BackgroundFetchError)> callback,
    blink::mojom::BackgroundFetchError error,
    FailureReason failure_reason) {
  DCHECK(callback);
  std::move(callback).Run(error);

  // It's normal to get INVALID_ID errors here - it means the registration was
  // already inactive (marked for deletion). This happens when an abort (from
  // developer or from user) races with the download completing/failing, or even
  // when two aborts race.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    return;

  auto controllers_iter = job_controllers_.find(registration_id.unique_id());
  DCHECK(controllers_iter != job_controllers_.end());

  failure_reason = controllers_iter->second->MergeFailureReason(failure_reason);
  blink::mojom::BackgroundFetchResult result =
      failure_reason == FailureReason::NONE
          ? blink::mojom::BackgroundFetchResult::SUCCESS
          : blink::mojom::BackgroundFetchResult::FAILURE;

  auto registration = controllers_iter->second->NewRegistration(result);
  DispatchCompletionEvent(registration_id, std::move(registration));
}

void BackgroundFetchContext::DispatchCompletionEvent(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  registration_notifier_->Notify(*registration);

  switch (registration->failure_reason) {
    case FailureReason::NONE:
      DCHECK_EQ(registration->result,
                blink::mojom::BackgroundFetchResult::SUCCESS);
      event_dispatcher_.DispatchBackgroundFetchSuccessEvent(
          registration_id, std::move(registration),
          base::BindOnce(&BackgroundFetchContext::CleanupRegistration,
                         weak_factory_.GetWeakPtr(), registration_id,
                         blink::mojom::BackgroundFetchResult::SUCCESS,
                         /* preserve_info_to_dispatch_click_event= */ true));
      return;
    case FailureReason::CANCELLED_FROM_UI:
    case FailureReason::CANCELLED_BY_DEVELOPER:
      DCHECK_EQ(registration->result,
                blink::mojom::BackgroundFetchResult::FAILURE);
      event_dispatcher_.DispatchBackgroundFetchAbortEvent(
          registration_id, std::move(registration),
          base::BindOnce(&BackgroundFetchContext::CleanupRegistration,
                         weak_factory_.GetWeakPtr(), registration_id,
                         blink::mojom::BackgroundFetchResult::FAILURE,
                         /* preserve_info_to_dispatch_click_event= */ false));
      return;
    case FailureReason::BAD_STATUS:
    case FailureReason::FETCH_ERROR:
    case FailureReason::SERVICE_WORKER_UNAVAILABLE:
    case FailureReason::QUOTA_EXCEEDED:
    case FailureReason::TOTAL_DOWNLOAD_SIZE_EXCEEDED:
      DCHECK_EQ(registration->result,
                blink::mojom::BackgroundFetchResult::FAILURE);
      event_dispatcher_.DispatchBackgroundFetchFailEvent(
          registration_id, std::move(registration),
          base::BindOnce(&BackgroundFetchContext::CleanupRegistration,
                         weak_factory_.GetWeakPtr(), registration_id,
                         blink::mojom::BackgroundFetchResult::FAILURE,
                         /* preserve_info_to_dispatch_click_event= */ true));
      return;
  }
}

void BackgroundFetchContext::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchResult background_fetch_result,
    bool preserve_info_to_dispatch_click_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Indicate to the renderer that the records for this fetch are no longer
  // available.
  registration_notifier_->NotifyRecordsUnavailable(registration_id.unique_id());

  // If we had an active JobController, it is no longer necessary, as the
  // notification's UI can no longer be updated after the fetch is aborted, or
  // after the waitUntil promise of the
  // backgroundfetchsuccess/backgroundfetchfail event has been resolved. Store
  // the information we want to persist after the controller is gone, in
  // completed_fetches_.
  auto controllers_iter = job_controllers_.find(registration_id.unique_id());
  DCHECK(controllers_iter != job_controllers_.end());
  if (preserve_info_to_dispatch_click_event) {
    completed_fetches_[registration_id.unique_id()] = std::make_pair(
        registration_id,
        controllers_iter->second->NewRegistration(background_fetch_result));
  }
  job_controllers_.erase(registration_id.unique_id());

  // Delete the data associated with this fetch. Cache storage will keep the
  // downloaded data around so long as there are references to it, and delete
  // it once there is none. We don't need to do that accounting.
  data_manager_->DeleteRegistration(
      registration_id,
      base::BindOnce(&background_fetch::RecordRegistrationDeletedError));
}

void BackgroundFetchContext::DispatchClickEvent(const std::string& unique_id) {
  auto iter = completed_fetches_.find(unique_id);
  if (iter != completed_fetches_.end()) {
    // The fetch has succeeded or failed. (not aborted/cancelled).
    event_dispatcher_.DispatchBackgroundFetchClickEvent(
        iter->second.first /* registration_id */,
        std::move(iter->second.second) /* registration */, base::DoNothing());
    completed_fetches_.erase(iter);
    return;
  }

  // The fetch is active.
  auto controllers_iter = job_controllers_.find(unique_id);
  if (controllers_iter == job_controllers_.end())
    return;
  auto registration = controllers_iter->second->NewRegistration(
      blink::mojom::BackgroundFetchResult::UNSET);
  event_dispatcher_.DispatchBackgroundFetchClickEvent(
      controllers_iter->second->registration_id(), std::move(registration),
      base::DoNothing());
}

void BackgroundFetchContext::MatchRequests(
    const BackgroundFetchRegistrationId& registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->MatchRequests(
      registration_id, std::move(match_params),
      base::BindOnce(&BackgroundFetchContext::DidGetMatchingRequests,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchContext::DidGetMatchingRequests(
    blink::mojom::BackgroundFetchService::MatchRequestsCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::vector<BackgroundFetchSettledFetch> settled_fetches) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/863016): Update to 0u once we've stopped sending an
  // uncached response.
  if (error != blink::mojom::BackgroundFetchError::NONE)
    DCHECK_EQ(settled_fetches.size(), 1u);

  std::move(callback).Run(std::move(settled_fetches));
}

void BackgroundFetchContext::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundFetchContext::ShutdownOnIO, this));
}

void BackgroundFetchContext::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_manager_->ShutdownOnIO();
}

void BackgroundFetchContext::SetDataManagerForTesting(
    std::unique_ptr<BackgroundFetchDataManager> data_manager) {
  DCHECK(data_manager);
  data_manager_ = std::move(data_manager);
  scheduler_ = std::make_unique<BackgroundFetchScheduler>(data_manager_.get());
}

}  // namespace content
