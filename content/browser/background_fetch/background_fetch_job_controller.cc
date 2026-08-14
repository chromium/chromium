// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/isolation_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace content {

namespace {

// Performs mixed content checks on the |request| for Background Fetch.
// Background Fetch depends on Service Workers, which are restricted for use
// on secure origins. We can therefore assume that the registration's origin
// is secure. This test ensures that the origin for the url of every
// request is also secure.
bool IsMixedContent(const BackgroundFetchRequestInfo& request) {
  // Empty request is valid, it shouldn't fail the mixed content check.
  if (request.fetch_request()->url.is_empty())
    return false;

  return !network::IsUrlPotentiallyTrustworthy(request.fetch_request()->url);
}

// Whether the |request| needs CORS preflight.
// Requests that require CORS preflights are temporarily blocked, because the
// browser side of Background Fetch doesn't yet support performing CORS
// checks. TODO(crbug.com/40515511): Remove this temporary block.
bool RequiresCorsPreflight(const BackgroundFetchRequestInfo& request,
                           const url::Origin& origin) {
  const blink::mojom::FetchAPIRequestPtr& fetch_request =
      request.fetch_request();

  // Same origin requests don't require a CORS preflight.
  // https://fetch.spec.whatwg.org/#main-fetch
  // TODO(crbug.com/40515511): Make sure that cross-origin redirects are
  // disabled.
  if (url::IsSameOriginWith(origin.GetURL(), fetch_request->url))
    return false;

  // Requests that are more involved than what is possible with HTML's form
  // element require a CORS-preflight request.
  // https://fetch.spec.whatwg.org/#main-fetch
  if (!fetch_request->method.empty() &&
      !network::cors::IsCorsSafelistedMethod(fetch_request->method)) {
    return true;
  }

  net::HttpRequestHeaders::HeaderVector headers;
  for (const auto& header : fetch_request->headers)
    headers.emplace_back(header.first, header.second);

  return !network::cors::CorsUnsafeRequestHeaderNames(headers).empty();
}

}  // namespace

using blink::mojom::BackgroundFetchError;
using blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDataManager* data_manager,
    BackgroundFetchDelegateProxy* delegate_proxy,
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    uint64_t bytes_downloaded,
    uint64_t bytes_uploaded,
    uint64_t upload_total,
    ProgressCallback progress_callback,
    FinishedCallback finished_callback)
    : data_manager_(data_manager),
      delegate_proxy_(delegate_proxy),
      registration_id_(registration_id),
      options_(std::move(options)),
      icon_(icon),
      complete_requests_downloaded_bytes_cache_(bytes_downloaded),
      complete_requests_uploaded_bytes_cache_(bytes_uploaded),
      upload_total_(upload_total),
      progress_callback_(std::move(progress_callback)),
      finished_callback_(std::move(finished_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BackgroundFetchJobController::InitializeRequestStatus(
    int completed_downloads,
    int total_downloads,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    bool start_paused,
    std::optional<net::IsolationInfo> isolation_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't allow double initialization.
  DCHECK_GT(total_downloads, 0);
  DCHECK_EQ(total_downloads_, 0);

  completed_downloads_ = completed_downloads;
  total_downloads_ = total_downloads;
  pending_downloads_ = active_fetch_requests.size();

  std::vector<std::string> active_guids;
  active_guids.reserve(active_fetch_requests.size());
  for (const auto& request_info : active_fetch_requests)
    active_guids.push_back(request_info->download_guid());

  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      registration_id().unique_id(), registration_id().storage_key().origin(),
      options_->title, icon_, completed_downloads_, total_downloads_,
      complete_requests_downloaded_bytes_cache_,
      complete_requests_uploaded_bytes_cache_, options_->download_total,
      upload_total_, std::move(active_guids), start_paused,
      std::move(isolation_info));

  for (auto& active_request : active_fetch_requests)
    active_request_map_[active_request->download_guid()] = active_request;

  delegate_proxy_->CreateDownloadJob(weak_ptr_factory_.GetWeakPtr(),
                                     std::move(fetch_description));
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool BackgroundFetchJobController::HasMoreRequests() {
  return completed_downloads_ + pending_downloads_ < total_downloads_;
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    RequestFinishedCallback request_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_LT(completed_downloads_, total_downloads_);
  DCHECK(request_finished_callback);
  DCHECK(request);

  active_request_finished_callbacks_.emplace(
      request->download_guid(), std::move(request_finished_callback));

  if (IsMixedContent(*request.get()) ||
      RequiresCorsPreflight(*request.get(),
                            registration_id_.storage_key().origin())) {
    request->SetEmptyResultWithFailureReason(
        BackgroundFetchResult::FailureReason::FETCH_ERROR);

    NotifyDownloadComplete(std::move(request));
    return;
  }

  active_request_map_[request->download_guid()] = request;
  delegate_proxy_->StartRequest(registration_id().unique_id(),
                                registration_id().storage_key().origin(),
                                request.get());
}

void BackgroundFetchJobController::DidStartRequest(
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(active_request_map_.count(guid));
  const auto& request = active_request_map_[guid];
  DCHECK(request);

  request->PopulateWithResponse(std::move(response));

  // TODO(crbug.com/40593934): Stop the fetch if the cross origin filter fails.
  BackgroundFetchCrossOriginFilter filter(
      registration_id_.storage_key().origin(), *request);
  request->set_can_populate_body(filter.CanPopulateBody());
  if (!request->can_populate_body())
    has_failed_cors_request_ = true;
}

void BackgroundFetchJobController::DidUpdateRequest(const std::string& guid,
                                                    uint64_t bytes_uploaded,
                                                    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(active_request_map_.count(guid));
  const auto& request = active_request_map_[guid];
  DCHECK(request);
  InProgressRequestBytes& in_progress_bytes = active_bytes_map_[guid];

  // Don't send download updates so the size is not leaked.
  // Upload updates are fine since that information is already available.
  if (!request->can_populate_body() && bytes_downloaded > 0u)
    return;

  if (in_progress_bytes.downloaded == bytes_downloaded &&
      in_progress_bytes.uploaded == bytes_uploaded) {
    return;
  }

  in_progress_bytes.downloaded = bytes_downloaded;
  in_progress_bytes.uploaded = bytes_uploaded;

  auto registration_data = NewRegistrationData();
  registration_data->downloaded += GetInProgressDownloadedBytes();
  registration_data->uploaded += GetInProgressUploadedBytes();
  progress_callback_.Run(registration_id_.unique_id(), *registration_data);
}

void BackgroundFetchJobController::DidCompleteRequest(
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(active_request_map_.count(guid));
  const auto& request = active_request_map_[guid];
  DCHECK(request);

  request->SetResult(std::move(result));

  if (request->can_populate_body())
    complete_requests_downloaded_bytes_cache_ += request->GetResponseSize();
  complete_requests_uploaded_bytes_cache_ += request->request_body_size();

  NotifyDownloadComplete(request);
  active_bytes_map_.erase(guid);
  active_request_map_.erase(guid);
}

blink::mojom::BackgroundFetchRegistrationDataPtr
BackgroundFetchJobController::NewRegistrationData() const {
  return blink::mojom::BackgroundFetchRegistrationData::New(
      registration_id().developer_id(), upload_total_,
      complete_requests_uploaded_bytes_cache_, options_->download_total,
      complete_requests_downloaded_bytes_cache_,
      blink::mojom::BackgroundFetchResult::UNSET, failure_reason_);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  uint64_t bytes = 0u;
  for (const std::pair<const std::string, InProgressRequestBytes>&
           in_progress_bytes : active_bytes_map_) {
    bytes += in_progress_bytes.second.downloaded;
  }
  return bytes;
}

uint64_t BackgroundFetchJobController::GetInProgressUploadedBytes() {
  uint64_t bytes = 0u;
  for (const std::pair<const std::string, InProgressRequestBytes>&
           in_progress_bytes : active_bytes_map_) {
    bytes += in_progress_bytes.second.uploaded;
  }
  return bytes;
}

void BackgroundFetchJobController::AbortFromDelegate(
    BackgroundFetchFailureReason failure_reason) {
  if (failure_reason == BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED &&
      has_failed_cors_request_) {
    // Don't expose that the download total has been exceeded. Use a less
    // specific error.
    failure_reason_ = BackgroundFetchFailureReason::FETCH_ERROR;
  } else {
    failure_reason_ = failure_reason;
  }

  Finish(failure_reason_, base::DoNothing());
}

void BackgroundFetchJobController::Abort(
    BackgroundFetchFailureReason failure_reason,
    ErrorCallback callback) {
  failure_reason_ = failure_reason;

  // Cancel any in-flight downloads and UI through the BGFetchDelegate.
  delegate_proxy_->Abort(registration_id().unique_id());

  Finish(failure_reason_, std::move(callback));
}

void BackgroundFetchJobController::Finish(
    BackgroundFetchFailureReason reason_to_abort,
    ErrorCallback callback) {
  DCHECK(reason_to_abort != BackgroundFetchFailureReason::NONE ||
         !HasMoreRequests());

  // Race conditions make it possible for a controller to finish twice. This
  // should be removed when the scheduler starts owning the controllers.
  if (!finished_callback_) {
    std::move(callback).Run(BackgroundFetchError::INVALID_ID);
    return;
  }

  std::move(finished_callback_)
      .Run(registration_id_, reason_to_abort, std::move(callback));
}

void BackgroundFetchJobController::PopNextRequest(
    RequestStartedCallback request_started_callback,
    RequestFinishedCallback request_finished_callback) {
  DCHECK(HasMoreRequests());

  ++pending_downloads_;
  data_manager_->PopNextRequest(
      registration_id(),
      base::BindOnce(&BackgroundFetchJobController::DidPopNextRequest,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_started_callback),
                     std::move(request_finished_callback)));
}

void BackgroundFetchJobController::DidPopNextRequest(
    RequestStartedCallback request_started_callback,
    RequestFinishedCallback request_finished_callback,
    BackgroundFetchError error,
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  if (error != BackgroundFetchError::NONE) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    return;
  }

  if (url_loader_factory_ ||
      !base::FeatureList::IsEnabled(
          features::kBackgroundFetchLocalNetworkAccess)) {
    if (url_loader_factory_) {
      request_info->set_url_loader_factory(url_loader_factory_);
    }
    std::move(request_started_callback)
        .Run(registration_id(), request_info.get());
    StartRequest(std::move(request_info), std::move(request_finished_callback));
    return;
  }

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      data_manager_->service_worker_context();
  if (service_worker_context) {
    service_worker_context->FindRegistrationForIdWithoutActivation(
        registration_id_.service_worker_registration_id(),
        registration_id_.storage_key(),
        base::BindOnce(
            &BackgroundFetchJobController::DidGetRegistrationForRequest,
            weak_ptr_factory_.GetWeakPtr(), std::move(request_started_callback),
            std::move(request_finished_callback), std::move(request_info)));
  } else {
    DidGetRegistrationForRequest(
        std::move(request_started_callback),
        std::move(request_finished_callback), std::move(request_info),
        blink::ServiceWorkerStatusCode::kErrorAbort, nullptr);
  }
}

void BackgroundFetchJobController::InitializeUrlLoaderFactory(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (url_loader_factory_ || !registration || !registration->active_version()) {
    return;
  }

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
      data_manager_->service_worker_context();
  if (!service_worker_context) {
    return;
  }

  StoragePartitionImpl* storage_partition =
      service_worker_context->storage_partition();
  if (!storage_partition) {
    return;
  }

  int process_id = ChildProcessHost::kInvalidUniqueID;
  if (registration->active_version()->embedded_worker()->status() ==
      blink::EmbeddedWorkerStatus::kRunning) {
    // TODO(crbug.com/379869738): Remove GetUnsafeValue once RendererProcessId
    // adopts strong types.
    process_id = registration->active_version()
                     ->embedded_worker()
                     ->process_id()
                     .GetUnsafeValue();
  }

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
      observer_remote =
          storage_partition
              ->CreateURLLoaderNetworkObserverForServiceOrSharedWorker(
                  network::OriginatingProcessId::renderer(
                      network::RendererProcessId(process_id)),
                  registration_id_.storage_key().origin());

  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      storage_partition->CreateURLLoaderFactoryParams();
  factory_params->client_security_state =
      registration->active_version()->BuildClientSecurityState().Clone();

  if (observer_remote) {
    factory_params->url_loader_network_observer = std::move(observer_remote);
  }

  bool bypass_redirect_checks = false;
  DCHECK(!url_loader_factory_);
  url_loader_factory_ = url_loader_factory::Create(
      ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource,
      url_loader_factory::TerminalParams::ForNetworkContext(
          storage_partition->GetNetworkContext(), std::move(factory_params),
          url_loader_factory::HeaderClientOption::kAllow,
          url_loader_factory::FactoryOverrideOption::kAllow),
      url_loader_factory::ContentClientParams(
          storage_partition->browser_context(), nullptr /* frame_host */,
          process_id, registration_id_.storage_key().origin(),
          net::IsolationInfo(), ukm::kInvalidSourceIdObj,
          &bypass_redirect_checks));
}

void BackgroundFetchJobController::DidGetRegistrationForRequest(
    RequestStartedCallback request_started_callback,
    RequestFinishedCallback request_finished_callback,
    scoped_refptr<BackgroundFetchRequestInfo> request_info,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  // If the fetch has already failed, abort. This can happen if the fetch was
  // cancelled while we were getting the registration.
  if (failure_reason_ != BackgroundFetchFailureReason::NONE) {
    Abort(failure_reason_, base::DoNothing());
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk || !registration ||
      !registration->active_version()) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kBackgroundFetchLocalNetworkAccess)) {
    InitializeUrlLoaderFactory(registration);
    if (!url_loader_factory_) {
      Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
            base::DoNothing());
      return;
    }
    request_info->set_url_loader_factory(url_loader_factory_);
  }

  std::move(request_started_callback)
      .Run(registration_id(), request_info.get());
  StartRequest(std::move(request_info), std::move(request_finished_callback));
}

void BackgroundFetchJobController::MarkRequestAsComplete(
    scoped_refptr<BackgroundFetchRequestInfo> request_info) {
  data_manager_->MarkRequestAsComplete(
      registration_id(), std::move(request_info),
      base::BindOnce(&BackgroundFetchJobController::DidMarkRequestAsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchJobController::DidMarkRequestAsComplete(
    BackgroundFetchError error) {
  switch (error) {
    case BackgroundFetchError::NONE:
      break;
    case BackgroundFetchError::STORAGE_ERROR:
      Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
            base::DoNothing());
      return;
    case BackgroundFetchError::QUOTA_EXCEEDED:
      Abort(BackgroundFetchFailureReason::QUOTA_EXCEEDED, base::DoNothing());
      return;
    default:
      NOTREACHED();
  }

  if (completed_downloads_ == total_downloads_) {
    Finish(BackgroundFetchFailureReason::NONE, base::DoNothing());
    return;
  }
}

void BackgroundFetchJobController::NotifyDownloadComplete(
    scoped_refptr<BackgroundFetchRequestInfo> request) {
  --pending_downloads_;
  ++completed_downloads_;
  auto it = active_request_finished_callbacks_.find(request->download_guid());
  CHECK(it != active_request_finished_callbacks_.end());
  std::move(it->second).Run(registration_id(), std::move(request));
  active_request_finished_callbacks_.erase(it);
}

void BackgroundFetchJobController::DidGetRegistrationForUploadData(
    const std::string& guid,
    BackgroundFetchDelegate::GetUploadDataCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  if (status == blink::ServiceWorkerStatusCode::kOk && registration) {
    InitializeUrlLoaderFactory(registration);
  }
  if (!url_loader_factory_) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    std::move(callback).Run(
        BackgroundFetchDelegate::Client::GetUploadDataResponse());
    return;
  }
  GetUploadData(guid, std::move(callback));
}

void BackgroundFetchJobController::GetUploadData(
    const std::string& guid,
    BackgroundFetchDelegate::GetUploadDataCallback callback) {
  auto it = active_request_map_.find(guid);
  if (it == active_request_map_.end() || !it->second) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    std::move(callback).Run(
        BackgroundFetchDelegate::Client::GetUploadDataResponse());
    return;
  }
  const scoped_refptr<BackgroundFetchRequestInfo>& request = it->second;

  if (base::FeatureList::IsEnabled(
          features::kBackgroundFetchLocalNetworkAccess) &&
      !url_loader_factory_) {
    // If the factory is missing (e.g. after restart), we need to recreate it
    // before returning the upload data.
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context =
        data_manager_->service_worker_context();
    if (!service_worker_context) {
      Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
            base::DoNothing());
      std::move(callback).Run(
          BackgroundFetchDelegate::Client::GetUploadDataResponse());
      return;
    }

    service_worker_context->FindRegistrationForIdWithoutActivation(
        registration_id_.service_worker_registration_id(),
        registration_id_.storage_key(),
        base::BindOnce(
            &BackgroundFetchJobController::DidGetRegistrationForUploadData,
            weak_ptr_factory_.GetWeakPtr(), guid, std::move(callback)));
    return;
  }

  if (request->request_body_size() == 0) {
    DidGetUploadData(std::move(callback), BackgroundFetchError::NONE, nullptr);
    return;
  }

  data_manager_->GetRequestBlob(
      registration_id(), request,
      base::BindOnce(&BackgroundFetchJobController::DidGetUploadData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundFetchJobController::DidGetUploadData(
    BackgroundFetchDelegate::GetUploadDataCallback callback,
    BackgroundFetchError error,
    blink::mojom::SerializedBlobPtr blob) {
  if (error != BackgroundFetchError::NONE) {
    Abort(BackgroundFetchFailureReason::SERVICE_WORKER_UNAVAILABLE,
          base::DoNothing());
    std::move(callback).Run(
        BackgroundFetchDelegate::Client::GetUploadDataResponse());
    return;
  }

  BackgroundFetchDelegate::Client::GetUploadDataResponse response;
  response.blob = std::move(blob);
  response.url_loader_factory = url_loader_factory_;

  std::move(callback).Run(std::move(response));
}

}  // namespace content
