// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_registration_service_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

namespace {

// The maximum number of active registrations that can be processed
// concurrently. The active registrations are from distinct origins.
constexpr char kMaxActiveRegistrations[] = "max_active_registrations";
constexpr int kMaxActiveRegistrationsDefaultValue = 2;

// The maximum number of downloads the Download Service can process at the same
// time.
// TODO(crbug.com/40608586): Figure out how to keep this in sync with the
// Download Service value.
constexpr char kMaxRunningDownloads[] = "max_running_downloads";
constexpr int kMaxRunningDownloadsDefaultValue = 2;

}  // namespace

using blink::mojom::BackgroundFetchError;
using blink::mojom::BackgroundFetchFailureReason;

// The major stages/events a Background Fetch instance goes through via the
// BackgroundFetchScheduler.
enum class BackgroundFetchScheduler::Event {
  // The Background Fetch was successfully registered.
  kFetchRegistered,
  // The Background Fetch registration was loaded on start-up.
  kFetchResumedOnStartup,
  // The scheduler marked the registration as active.
  kFetchScheduled,
  // A request within the registration is being fetched.
  kRequestStarted,
  // A request within the registration had been fetched.
  kRequestCompleted,
};

BackgroundFetchScheduler::BackgroundFetchScheduler(
    BackgroundFetchContext* background_fetch_context,
    BackgroundFetchDataManager* data_manager,
    BackgroundFetchRegistrationNotifier* registration_notifier,
    BackgroundFetchDelegateProxy* delegate_proxy,
    DevToolsBackgroundServicesContextImpl& devtools_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : data_manager_(data_manager),
      registration_notifier_(registration_notifier),
      delegate_proxy_(delegate_proxy),
      devtools_context_(&devtools_context),
      event_dispatcher_(background_fetch_context,
                        std::move(service_worker_context),
                        devtools_context) {
  DCHECK(delegate_proxy_);
  delegate_proxy_->SetClickEventDispatcher(
      base::BindRepeating(&BackgroundFetchScheduler::DispatchClickEvent,
                          weak_ptr_factory_.GetWeakPtr()));

  max_active_registrations_ = base::GetFieldTrialParamByFeatureAsInt(
      features::kBackgroundFetch, kMaxActiveRegistrations,
      kMaxActiveRegistrationsDefaultValue);
  max_running_downloads_ = base::GetFieldTrialParamByFeatureAsInt(
      features::kBackgroundFetch, kMaxRunningDownloads,
      kMaxRunningDownloadsDefaultValue);
}

BackgroundFetchScheduler::~BackgroundFetchScheduler() = default;

BackgroundFetchScheduler::RegistrationData::RegistrationData(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationDataPtr registration)
    : registration_id(registration_id), registration(std::move(registration)) {}

BackgroundFetchScheduler::RegistrationData::~RegistrationData() = default;

bool BackgroundFetchScheduler::ScheduleDownload() {
  DCHECK_LT(num_running_downloads_, max_running_downloads_);

  // 1. Try to activate a registration from a different storage key.
  if (num_active_registrations_ < max_active_registrations_ &&
      !controller_ids_.empty()) {
    // Try to find a pending registration with a different storage key.
    for (const auto& controller_id : controller_ids_) {
      // Make sure the storage key is not already active.
      bool is_new_storage_key = true;
      for (auto* controller : active_controllers_) {
        if (controller->registration_id().storage_key() ==
            controller_id.storage_key()) {
          is_new_storage_key = false;
          break;
        }
      }

      if (is_new_storage_key) {
        // Start new registration, and move to the front of the queue.
        auto* controller = job_controllers_[controller_id.unique_id()].get();
        active_controllers_.push_front(controller);
        ++num_active_registrations_;

        LogBackgroundFetchEventForDevTools(Event::kFetchScheduled,
                                           controller_id,
                                           /* request_info= */ nullptr);

        base::Erase(controller_ids_, controller_id);
        break;
      }
    }
  }

  // 2. Try to start a request within the LRU registration.
  for (auto it = active_controllers_.begin(); it != active_controllers_.end();
       ++it) {
    auto* controller = *it;
    if (!controller->HasMoreRequests())
      continue;

    // Activate a request within |controller| and move it to the end of the
    // queue.
    ++num_running_downloads_;
    controller->PopNextRequest(
        base::BindOnce(&BackgroundFetchScheduler::DidStartRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BackgroundFetchScheduler::DidCompleteRequest,
                       weak_ptr_factory_.GetWeakPtr()));

    active_controllers_.erase(it);
    active_controllers_.push_back(controller);
    return true;
  }
  return false;
}

void BackgroundFetchScheduler::Abort(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchFailureReason failure_reason,
    blink::mojom::BackgroundFetchRegistrationService::AbortCallback callback) {
  DCHECK_EQ(failure_reason,
            BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER);

  base::Erase(controller_ids_, registration_id);

  auto it = job_controllers_.find(registration_id.unique_id());
  if (it == job_controllers_.end()) {
    std::move(callback).Run(BackgroundFetchError::INVALID_ID);
    return;
  }

  it->second->Abort(failure_reason, std::move(callback));
}

void BackgroundFetchScheduler::DidStartRequest(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRequestInfo* request_info) {
  LogBackgroundFetchEventForDevTools(Event::kRequestStarted, registration_id,
                                     request_info);
}

void BackgroundFetchScheduler::DidCompleteRequest(
    const BackgroundFetchRegistrationId& registration_id,
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  LogBackgroundFetchEventForDevTools(Event::kRequestCompleted, registration_id,
                                     request_info.get());

  auto* controller = GetActiveController(registration_id);
  if (controller)
    controller->MarkRequestAsComplete(std::move(request_info));

  --num_running_downloads_;
  if (num_running_downloads_ < max_running_downloads_)
    ScheduleDownload();
}

void BackgroundFetchScheduler::FinishJob(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchFailureReason failure_reason,
    base::OnceCallback<void(BackgroundFetchError)> callback) {
  auto* active_controller = GetActiveController(registration_id);
  if (active_controller) {
    base::EraseIf(active_controllers_, [&registration_id](auto* controller) {
      return controller->registration_id() == registration_id;
    });
  }

  data_manager_->MarkRegistrationForDeletion(
      registration_id,
      /* check_for_failure= */ failure_reason ==
          BackgroundFetchFailureReason::NONE,
      base::BindOnce(&BackgroundFetchScheduler::DidMarkForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), registration_id,
                     /* job_started= */ active_controller != nullptr,
                     std::move(callback)));

  auto it = job_controllers_.find(registration_id.unique_id());
  if (it != job_controllers_.end()) {
    completed_fetches_[it->first] = std::make_unique<RegistrationData>(
        registration_id, it->second->NewRegistrationData());

    // Reset scheduler params.
    num_running_downloads_ -= it->second->pending_downloads();
    --num_active_registrations_;

    // Destroying the controller will stop all in progress tasks.
    job_controllers_.erase(it);
  }

  if (num_running_downloads_ < max_running_downloads_)
    ScheduleDownload();
}

void BackgroundFetchScheduler::DidMarkForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool job_started,
    base::OnceCallback<void(BackgroundFetchError)> callback,
    BackgroundFetchError error,
    BackgroundFetchFailureReason failure_reason) {
  DCHECK(callback);
  std::move(callback).Run(error);

  // It's normal to get INVALID_ID errors here - it means the registration was
  // already inactive (marked for deletion). This happens when an abort (from
  // developer or from user) races with the download completing/failing, or even
  // when two aborts race.
  if (error != BackgroundFetchError::NONE)
    return;

  auto it = completed_fetches_.find(registration_id.unique_id());
  CHECK(it != completed_fetches_.end(), base::NotFatalUntil::M130);

  blink::mojom::BackgroundFetchRegistrationDataPtr& registration_data =
      it->second->registration;
  // Include any other failure reasons the marking for deletion may have found.
  if (registration_data->failure_reason == BackgroundFetchFailureReason::NONE)
    registration_data->failure_reason = failure_reason;

  registration_data->result =
      registration_data->failure_reason == BackgroundFetchFailureReason::NONE
          ? blink::mojom::BackgroundFetchResult::SUCCESS
          : blink::mojom::BackgroundFetchResult::FAILURE;

  registration_notifier_->Notify(registration_id.unique_id(),
                                 *registration_data);

  event_dispatcher_.DispatchBackgroundFetchCompletionEvent(
      registration_id, registration_data.Clone(),
      base::BindOnce(&BackgroundFetchScheduler::CleanupRegistration,
                     weak_ptr_factory_.GetWeakPtr(), registration_id));

  if (!job_started ||
      registration_data->failure_reason ==
          BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
      registration_data->failure_reason ==
          BackgroundFetchFailureReason::CANCELLED_BY_DEVELOPER) {
    // No need to keep the controller around since there won't be dispatch
    // events.
    completed_fetches_.erase(it);
  } else {
    // The registration is now safe to delete.
    it->second->processing_completed = true;
  }
}

void BackgroundFetchScheduler::CleanupRegistration(
    const BackgroundFetchRegistrationId& registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Indicate to the renderer that the records for this fetch are no longer
  // available.
  registration_notifier_->NotifyRecordsUnavailable(registration_id.unique_id());

  // Delete the data associated with this fetch. Cache storage will keep the
  // downloaded data around so long as there are references to it, and delete
  // it once there is none. We don't need to do that accounting.
  data_manager_->DeleteRegistration(registration_id, base::DoNothing());

  // Notify other systems that this registration is complete.
  delegate_proxy_->MarkJobComplete(registration_id.unique_id());
}

void BackgroundFetchScheduler::DispatchClickEvent(
    const std::string& unique_id) {
  // Case 1: The active fetch received a click event.
  auto* active_controller = GetActiveController(unique_id);

  if (active_controller) {
    event_dispatcher_.DispatchBackgroundFetchClickEvent(
        active_controller->registration_id(),
        active_controller->NewRegistrationData(), base::DoNothing());
    return;
  }

  // Case 2: A completed fetch received a click event.
  auto it = completed_fetches_.find(unique_id);
  if (it == completed_fetches_.end())
    return;

  event_dispatcher_.DispatchBackgroundFetchClickEvent(
      it->second->registration_id, it->second->registration.Clone(),
      base::DoNothing());

  if (it->second->processing_completed)
    completed_fetches_.erase(unique_id);
}

std::unique_ptr<BackgroundFetchJobController>
BackgroundFetchScheduler::CreateInitializedController(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistrationData& registration_data,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_completed_requests,
    int num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    bool start_paused,
    std::optional<net::IsolationInfo> isolation_info) {
  // TODO(rayankans): Only create a controller when the fetch starts.
  auto controller = std::make_unique<BackgroundFetchJobController>(
      data_manager_, delegate_proxy_, registration_id, std::move(options), icon,
      registration_data.downloaded, registration_data.uploaded,
      registration_data.upload_total,
      // Safe because JobControllers are destroyed before RegistrationNotifier.
      base::BindRepeating(&BackgroundFetchRegistrationNotifier::Notify,
                          base::Unretained(registration_notifier_)),
      base::BindOnce(&BackgroundFetchScheduler::FinishJob,
                     weak_ptr_factory_.GetWeakPtr()));

  controller->InitializeRequestStatus(num_completed_requests, num_requests,
                                      std::move(active_fetch_requests),
                                      start_paused, std::move(isolation_info));

  return controller;
}

void BackgroundFetchScheduler::OnRegistrationCreated(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistrationData& registration_data,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_requests,
    bool start_paused,
    net::IsolationInfo isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LogBackgroundFetchEventForDevTools(
      Event::kFetchRegistered, registration_id,
      /* request_info= */ nullptr,
      {{"Total Requests", base::NumberToString(num_requests)},
       {"Start Paused", start_paused ? "Yes" : "No"}});

  auto controller = CreateInitializedController(
      registration_id, registration_data, std::move(options), icon,
      /* completed_requests= */ 0, num_requests,
      /* active_fetch_requests= */ {}, start_paused, std::move(isolation_info));

  DCHECK_EQ(job_controllers_.count(registration_id.unique_id()), 0u);
  job_controllers_[registration_id.unique_id()] = std::move(controller);
  controller_ids_.push_back(registration_id);

  // Schedule as much as possible.
  while (num_running_downloads_ < max_running_downloads_) {
    if (!ScheduleDownload())
      return;
  }
}

void BackgroundFetchScheduler::OnRegistrationLoadedAtStartup(
    const BackgroundFetchRegistrationId& registration_id,
    const blink::mojom::BackgroundFetchRegistrationData& registration_data,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    int num_completed_requests,
    int num_requests,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    std::optional<net::IsolationInfo> isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LogBackgroundFetchEventForDevTools(
      Event::kFetchResumedOnStartup, registration_id,
      /* request_info= */ nullptr,
      {{"Completed Requests", base::NumberToString(num_completed_requests)},
       {"Active Requests",
        base::NumberToString(active_fetch_requests.size())}});

  auto controller = CreateInitializedController(
      registration_id, registration_data, std::move(options), icon,
      num_completed_requests, num_requests, active_fetch_requests,
      /* start_paused= */ false, std::move(isolation_info));

  auto* controller_ptr = controller.get();
  active_controllers_.push_back(controller_ptr);
  job_controllers_[registration_id.unique_id()] = std::move(controller);

  ++num_active_registrations_;
  num_running_downloads_ += active_fetch_requests.size();

  if (active_fetch_requests.empty()) {
    DCHECK_LT(num_completed_requests, num_requests);
    // Start processing the next request.
    ++num_running_downloads_;
    controller_ptr->PopNextRequest(
        base::BindOnce(&BackgroundFetchScheduler::DidStartRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&BackgroundFetchScheduler::DidCompleteRequest,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  for (auto& request_info : active_fetch_requests) {
    controller_ptr->StartRequest(
        std::move(request_info),
        base::BindOnce(&BackgroundFetchScheduler::DidCompleteRequest,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BackgroundFetchScheduler::OnRequestCompleted(
    const std::string& unique_id,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response) {
  registration_notifier_->NotifyRequestCompleted(unique_id, std::move(request),
                                                 std::move(response));
}

void BackgroundFetchScheduler::AbortFetches(
    int64_t service_worker_registration_id) {
  // Abandon all active associated with this service worker.
  // BackgroundFetchJobController::Abort() will eventually lead to deletion of
  // the controller from job_controllers, so the IDs need to be copied over.
  std::vector<BackgroundFetchJobController*> to_abort;
  for (const auto& controller : job_controllers_) {
    if (service_worker_registration_id !=
            blink::mojom::kInvalidServiceWorkerRegistrationId &&
        service_worker_registration_id !=
            controller.second->registration_id()
                .service_worker_registration_id()) {
      continue;
    }
    to_abort.push_back(controller.second.get());
  }

  for (auto* controller : to_abort) {
    // Erase it from |controller_ids_| first to avoid rescheduling.
    base::Erase(controller_ids_, controller->registration_id());
    controller->Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
                      base::DoNothing());
  }
}

void BackgroundFetchScheduler::OnRegistrationQueried(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchRegistrationData* registration_data) {
  DCHECK(registration_data);

  auto* controller = GetActiveController(registration_id.unique_id());
  if (!controller)
    return;

  // The data manager only has the number of bytes from completed downloads, so
  // augment this with the number of downloaded/uploaded bytes from in-progress
  // jobs.
  registration_data->downloaded += controller->GetInProgressDownloadedBytes();
  registration_data->uploaded += controller->GetInProgressUploadedBytes();
}

void BackgroundFetchScheduler::OnServiceWorkerDatabaseCorrupted(
    int64_t service_worker_registration_id) {
  AbortFetches(service_worker_registration_id);
}

void BackgroundFetchScheduler::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern,
    const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AbortFetches(registration_id);
}

void BackgroundFetchScheduler::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AbortFetches(blink::mojom::kInvalidServiceWorkerRegistrationId);
}

BackgroundFetchJobController* BackgroundFetchScheduler::GetActiveController(
    const BackgroundFetchRegistrationId& registration_id) {
  for (auto* controller : active_controllers_) {
    if (controller->registration_id() == registration_id)
      return controller;
  }
  return nullptr;
}

BackgroundFetchJobController* BackgroundFetchScheduler::GetActiveController(
    const std::string& unique_id) {
  // |unique_id| is used for all BackgroundFetchRegistrationId comparisons, so
  // this creates a |unique_id| wrapper with default values for other
  // parameters.
  BackgroundFetchRegistrationId registration_id(
      /* service_worker_registration_id= */ 0,
      /* storage_key= */ blink::StorageKey(),
      /* developer_id= */ "", unique_id);
  return GetActiveController(registration_id);
}

void BackgroundFetchScheduler::LogBackgroundFetchEventForDevTools(
    Event event,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRequestInfo* request_info,
    std::map<std::string, std::string> metadata) {
  CHECK(devtools_context_);
  if (!devtools_context_->IsRecording(
          DevToolsBackgroundService::kBackgroundFetch)) {
    return;
  }

  std::string event_name;

  // Fill with the appropriate event description in |event_name|, and append
  // any additional data to |metadata|.
  switch (event) {
    case Event::kFetchRegistered:
      event_name = "Background Fetch registered";
      break;
    case Event::kFetchResumedOnStartup:
      event_name = "Background Fetch resuming after browser restart";
      break;
    case Event::kFetchScheduled:
      event_name = "Background Fetch started";
      break;
    case Event::kRequestStarted:
      event_name = "Request processing started";
      DCHECK(request_info);
      break;
    case Event::kRequestCompleted:
      event_name = "Request processing completed";
      DCHECK(request_info);
      metadata["Response Status"] =
          base::NumberToString(request_info->GetResponseCode());
      metadata["Response Size (bytes)"] =
          base::NumberToString(request_info->GetResponseSize());
      break;
  }

  DCHECK(!event_name.empty());

  // Include common request metadata.
  if (request_info) {
    metadata["URL"] = request_info->fetch_request()->url.spec();
    metadata["Request Index"] =
        base::NumberToString(request_info->request_index());
    if (request_info->request_body_size())
      metadata["Upload Size (bytes)"] =
          base::NumberToString(request_info->request_body_size());
  }

  devtools_context_->LogBackgroundServiceEvent(
      registration_id.service_worker_registration_id(),
      registration_id.storage_key(),
      DevToolsBackgroundService::kBackgroundFetch, std::move(event_name),
      /* instance_id= */ registration_id.developer_id(), metadata);
}

void BackgroundFetchScheduler::Shutdown() {
  event_dispatcher_.Shutdown();
  devtools_context_ = nullptr;
}

}  // namespace content
