// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/request_priority.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/throttling/throttling_controller.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

namespace {

void NotifyNavigationPreloadRequestSentOnUI(
    const network::ResourceRequest& request,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->NavigationPreloadRequestSent(
      worker_id.first, worker_id.second, request_id, request);
}

void NotifyNavigationPreloadResponseReceivedOnUI(
    const GURL& url,
    scoped_refptr<network::ResourceResponse> response,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()
      ->NavigationPreloadResponseReceived(worker_id.first, worker_id.second,
                                          request_id, url, response->head);
}

void NotifyNavigationPreloadCompletedOnUI(
    const network::URLLoaderCompletionStatus& status,
    const std::pair<int, int>& worker_id,
    const std::string& request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->NavigationPreloadCompleted(
      worker_id.first, worker_id.second, request_id, status);
}

// DelegatingURLLoaderClient is the URLLoaderClient for the navigation preload
// network request. It watches as the response comes in, and pipes the response
// back to the service worker while also doing extra processing like notifying
// DevTools.
class DelegatingURLLoaderClient final : public network::mojom::URLLoaderClient {
 public:
  using WorkerId = std::pair<int, int>;
  explicit DelegatingURLLoaderClient(network::mojom::URLLoaderClientPtr client,
                                     base::OnceClosure on_response,
                                     const network::ResourceRequest& request)
      : binding_(this),
        client_(std::move(client)),
        on_response_(std::move(on_response)),
        url_(request.url),
        devtools_enabled_(request.report_raw_headers) {
    if (!devtools_enabled_)
      return;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadRequestSentOnUI, request));
  }
  ~DelegatingURLLoaderClient() override {
    if (!completed_) {
      // Let the service worker know that the request has been canceled.
      network::URLLoaderCompletionStatus status;
      status.error_code = net::ERR_ABORTED;
      client_->OnComplete(status);
      if (!devtools_enabled_)
        return;
      AddDevToolsCallback(
          base::BindOnce(&NotifyNavigationPreloadCompletedOnUI, status));
    }
  }

  void MaybeReportToDevTools(WorkerId worker_id, int fetch_event_id) {
    worker_id_ = worker_id;
    devtools_request_id_ = base::StringPrintf("preload-%d", fetch_event_id);
    MaybeRunDevToolsCallbacks();
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    client_->OnUploadProgress(current_position, total_size,
                              std::move(ack_callback));
  }
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    client_->OnReceiveCachedMetadata(data);
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    client_->OnTransferSizeUpdated(transfer_size_diff);
  }
  void OnReceiveResponse(const network::ResourceResponseHead& head) override {
    client_->OnReceiveResponse(head);
    DCHECK(on_response_);
    std::move(on_response_).Run();
    if (!devtools_enabled_)
      return;
    // Make a deep copy of ResourceResponseHead before passing it cross-thread.
    auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
    resource_response->head = head;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadResponseReceivedOnUI, url_,
                       resource_response->DeepCopy()));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override {
    completed_ = true;
    // When the server returns a redirect response, we only send
    // OnReceiveRedirect IPC and don't send OnComplete IPC. The service worker
    // will clean up the preload request when OnReceiveRedirect() is called.
    client_->OnReceiveRedirect(redirect_info, head);

    if (!devtools_enabled_)
      return;
    // Make a deep copy of ResourceResponseHead before passing it cross-thread.
    auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
    resource_response->head = head;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadResponseReceivedOnUI, url_,
                       resource_response->DeepCopy()));
    network::URLLoaderCompletionStatus status;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadCompletedOnUI, status));
  }
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    client_->OnStartLoadingResponseBody(std::move(body));
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (completed_)
      return;
    completed_ = true;
    client_->OnComplete(status);
    if (!devtools_enabled_)
      return;
    AddDevToolsCallback(
        base::BindOnce(&NotifyNavigationPreloadCompletedOnUI, status));
  }

  void Bind(network::mojom::URLLoaderClientPtr* ptr_info) {
    binding_.Bind(mojo::MakeRequest(ptr_info));
  }

 private:
  void MaybeRunDevToolsCallbacks() {
    if (!worker_id_ || !devtools_enabled_)
      return;
    while (!devtools_callbacks.empty()) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(std::move(devtools_callbacks.front()), *worker_id_,
                         devtools_request_id_));
      devtools_callbacks.pop();
    }
  }
  void AddDevToolsCallback(
      base::OnceCallback<void(const WorkerId&, const std::string&)> callback) {
    devtools_callbacks.push(std::move(callback));
    MaybeRunDevToolsCallbacks();
  }

  mojo::Binding<network::mojom::URLLoaderClient> binding_;
  network::mojom::URLLoaderClientPtr client_;
  base::OnceClosure on_response_;
  bool completed_ = false;
  const GURL url_;
  const bool devtools_enabled_;

  base::Optional<std::pair<int, int>> worker_id_;
  std::string devtools_request_id_;
  base::queue<base::OnceCallback<void(const WorkerId&, const std::string&)>>
      devtools_callbacks;
  DISALLOW_COPY_AND_ASSIGN(DelegatingURLLoaderClient);
};

using EventType = ServiceWorkerMetrics::EventType;
EventType ResourceTypeToEventType(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return EventType::FETCH_MAIN_FRAME;
    case RESOURCE_TYPE_SUB_FRAME:
      return EventType::FETCH_SUB_FRAME;
    case RESOURCE_TYPE_SHARED_WORKER:
      return EventType::FETCH_SHARED_WORKER;
    case RESOURCE_TYPE_SERVICE_WORKER:
    case RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED() << resource_type;
      return EventType::FETCH_SUB_RESOURCE;
    default:
      return EventType::FETCH_SUB_RESOURCE;
  }
}

std::unique_ptr<base::Value> NetLogServiceWorkerStatusCallback(
    blink::ServiceWorkerStatusCode status,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", blink::ServiceWorkerStatusToString(status));
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogFetchEventCallback(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult result,
    net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("status", blink::ServiceWorkerStatusToString(status));
  dict->SetBoolean(
      "has_response",
      result == ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse);
  return std::move(dict);
}

void EndNetLogEventWithServiceWorkerStatus(
    const net::NetLogWithSource& net_log,
    net::NetLogEventType type,
    blink::ServiceWorkerStatusCode status) {
  net_log.EndEvent(type,
                   base::Bind(&NetLogServiceWorkerStatusCallback, status));
}

const net::NetworkTrafficAnnotationTag kNavigationPreloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("service_worker_navigation_preload",
                                        R"(
    semantics {
      sender: "Service Worker Navigation Preload"
      description:
        "This request is issued by a navigation to fetch the content of the "
        "page that is being navigated to, in the case where a service worker "
        "has been registered for the page and is using the Navigation Preload "
        "API."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL, HTTP "
        "headers, and request body. Requests may include cookies and "
        "site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This request can be prevented by disabling service workers, which can "
        "be done by disabling cookie and site data under Settings, Content "
        "Settings, Cookies."
      chrome_policy {
        URLBlacklist {
          URLBlacklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLWhitelist {
          URLWhitelist { }
        }
      }
    }
    comments:
      "Chrome would be unable to use service workers if this feature were "
      "disabled, which could result in a degraded experience for websites that "
      "register a service worker. Using either URLBlacklist or URLWhitelist "
      "policies (or a combination of both) limits the scope of these requests."
)");

}  // namespace

// ResponseCallback is owned by the callback that is passed to
// ServiceWorkerVersion::StartRequest*(), and held in pending_requests_
// until FinishRequest() is called.
class ServiceWorkerFetchDispatcher::ResponseCallback
    : public blink::mojom::ServiceWorkerFetchResponseCallback {
 public:
  ResponseCallback(
      blink::mojom::ServiceWorkerFetchResponseCallbackRequest request,
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version)
      : binding_(this, std::move(request)),
        fetch_dispatcher_(fetch_dispatcher),
        version_(version) {}

  ~ResponseCallback() override { DCHECK(fetch_event_id_.has_value()); }

  void set_fetch_event_id(int id) {
    DCHECK(!fetch_event_id_);
    fetch_event_id_ = id;
  }

  // Implements blink::mojom::ServiceWorkerFetchResponseCallback.
  void OnResponse(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   std::move(response), nullptr /* body_as_stream */,
                   FetchEventResult::kGotResponse, std::move(timing));
  }
  void OnResponseStream(
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   std::move(response), std::move(body_as_stream),
                   FetchEventResult::kGotResponse, std::move(timing));
  }
  void OnFallback(
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) override {
    HandleResponse(fetch_dispatcher_, version_, fetch_event_id_,
                   blink::mojom::FetchAPIResponse::New(),
                   nullptr /* body_as_stream */,
                   FetchEventResult::kShouldFallback, std::move(timing));
  }

 private:
  // static as version->FinishRequest will remove the calling ResponseCallback
  // instance.
  static void HandleResponse(
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version,
      base::Optional<int> fetch_event_id,
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      FetchEventResult fetch_result,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
    if (!version->FinishRequest(fetch_event_id.value(),
                                fetch_result == FetchEventResult::kGotResponse,
                                timing->dispatch_event_time))
      NOTREACHED() << "Should only receive one reply per event";
    // |fetch_dispatcher| is null if the URLRequest was killed.
    if (!fetch_dispatcher)
      return;
    fetch_dispatcher->DidFinish(fetch_event_id.value(), fetch_result,
                                std::move(response), std::move(body_as_stream),
                                std::move(timing));
  }

  mojo::Binding<blink::mojom::ServiceWorkerFetchResponseCallback> binding_;
  base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  // Owns |this| via pending_requests_.
  ServiceWorkerVersion* version_;
  // Must be set to a non-nullopt value before the corresponding mojo
  // handle is passed to the other end (i.e. before any of OnResponse*
  // is called).
  base::Optional<int> fetch_event_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

// This class keeps the URL loader related assets alive while the FetchEvent is
// ongoing in the service worker.
class ServiceWorkerFetchDispatcher::URLLoaderAssets
    : public base::RefCounted<ServiceWorkerFetchDispatcher::URLLoaderAssets> {
 public:
  URLLoaderAssets(
      std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory,
      std::unique_ptr<DelegatingURLLoaderClient> url_loader_client)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_loader_client_(std::move(url_loader_client)) {}

  void MaybeReportToDevTools(std::pair<int, int> worker_id,
                             int fetch_event_id) {
    url_loader_client_->MaybeReportToDevTools(worker_id, fetch_event_id);
  }

 private:
  friend class base::RefCounted<URLLoaderAssets>;
  virtual ~URLLoaderAssets() {}

  std::unique_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<DelegatingURLLoaderClient> url_loader_client_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderAssets);
};

ServiceWorkerFetchDispatcher::ServiceWorkerFetchDispatcher(
    std::unique_ptr<network::ResourceRequest> request,
    const std::string& request_body_blob_uuid,
    uint64_t request_body_blob_size,
    blink::mojom::BlobPtr request_body_blob,
    const std::string& client_id,
    scoped_refptr<ServiceWorkerVersion> version,
    const net::NetLogWithSource& net_log,
    base::OnceClosure prepare_callback,
    FetchCallback fetch_callback)
    : request_(std::move(request)),
      request_body_blob_uuid_(request_body_blob_uuid),
      request_body_blob_size_(request_body_blob_size),
      request_body_blob_(std::move(request_body_blob)),
      client_id_(client_id),
      version_(std::move(version)),
      resource_type_(static_cast<ResourceType>(request_->resource_type)),
      net_log_(net_log),
      prepare_callback_(std::move(prepare_callback)),
      fetch_callback_(std::move(fetch_callback)),
      did_complete_(false),
      weak_factory_(this) {
#if DCHECK_IS_ON()
  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    DCHECK((request_body_blob_uuid_.empty() && request_body_blob_size_ == 0 &&
            !request_body_blob_ && client_id_.empty()));
  }
#endif  // DCHECK_IS_ON()
  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
                      net::NetLog::StringCallback(
                          "event_type", ServiceWorkerMetrics::EventTypeToString(
                                            GetEventType())));
}

ServiceWorkerFetchDispatcher::~ServiceWorkerFetchDispatcher() {
  if (!did_complete_)
    net_log_.EndEvent(
        net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT);
}

void ServiceWorkerFetchDispatcher::Run() {
  DCHECK(version_->status() == ServiceWorkerVersion::ACTIVATING ||
         version_->status() == ServiceWorkerVersion::ACTIVATED)
      << version_->status();

  if (version_->status() == ServiceWorkerVersion::ACTIVATING) {
    net_log_.BeginEvent(
        net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
    version_->RegisterStatusChangeCallback(
        base::BindOnce(&ServiceWorkerFetchDispatcher::DidWaitForActivation,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  StartWorker();
}

void ServiceWorkerFetchDispatcher::DidWaitForActivation() {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_WAIT_FOR_ACTIVATION);
  StartWorker();
}

void ServiceWorkerFetchDispatcher::StartWorker() {
  // We might be REDUNDANT if a new worker started activating and kicked us out
  // before we could finish activation.
  if (version_->status() != ServiceWorkerVersion::ACTIVATED) {
    DCHECK_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
    DidFail(blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed);
    return;
  }

  if (version_->running_status() == EmbeddedWorkerStatus::RUNNING) {
    DispatchFetchEvent();
    return;
  }

  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  version_->RunAfterStartWorker(
      GetEventType(),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidStartWorker,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerFetchDispatcher::DidStartWorker(
    blink::ServiceWorkerStatusCode status) {
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    EndNetLogEventWithServiceWorkerStatus(
        net_log_, net::NetLogEventType::SERVICE_WORKER_START_WORKER, status);
    DidFail(status);
    return;
  }
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_START_WORKER);
  DispatchFetchEvent();
}

void ServiceWorkerFetchDispatcher::DispatchFetchEvent() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status())
      << "Worker stopped too soon after it was started.";

  // Run callback to say that the fetch event will be dispatched.
  DCHECK(prepare_callback_);
  std::move(prepare_callback_).Run();
  net_log_.BeginEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);

  // Set up for receiving the response.
  blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback_ptr;
  auto response_callback = std::make_unique<ResponseCallback>(
      mojo::MakeRequest(&response_callback_ptr), weak_factory_.GetWeakPtr(),
      version_.get());
  ResponseCallback* response_callback_rawptr = response_callback.get();

  // Set up the fetch event.
  int fetch_event_id = version_->StartRequest(
      GetEventType(),
      base::BindOnce(&ServiceWorkerFetchDispatcher::DidFailToDispatch,
                     weak_factory_.GetWeakPtr(), std::move(response_callback)));
  int event_finish_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL, base::DoNothing());
  response_callback_rawptr->set_fetch_event_id(fetch_event_id);

  // Report navigation preload to DevTools if needed.
  if (url_loader_assets_) {
    url_loader_assets_->MaybeReportToDevTools(
        std::make_pair(
            version_->embedded_worker()->process_id(),
            version_->embedded_worker()->worker_devtools_agent_route_id()),
        fetch_event_id);
  }

  // Dispatch the fetch event.
  auto params = blink::mojom::DispatchFetchEventParams::New();
  params->request = *request_;
  params->request_body_blob_uuid = request_body_blob_uuid_;
  params->request_body_blob_size = request_body_blob_size_;
  params->request_body_blob = request_body_blob_.PassInterface();
  params->client_id = client_id_;
  params->preload_handle = std::move(preload_handle_);
  // |endpoint()| is owned by |version_|. So it is safe to pass the
  // unretained raw pointer of |version_| to OnFetchEventFinished callback.
  // Pass |url_loader_assets_| to the callback to keep the URL loader related
  // assets alive while the FetchEvent is ongoing in the service worker.
  version_->endpoint()->DispatchFetchEvent(
      std::move(params), std::move(response_callback_ptr),
      base::BindOnce(&ServiceWorkerFetchDispatcher::OnFetchEventFinished,
                     base::Unretained(version_.get()), event_finish_id,
                     url_loader_assets_));
}

void ServiceWorkerFetchDispatcher::DidFailToDispatch(
    std::unique_ptr<ResponseCallback> response_callback,
    blink::ServiceWorkerStatusCode status) {
  EndNetLogEventWithServiceWorkerStatus(
      net_log_, net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT, status);
  DidFail(status);
}

void ServiceWorkerFetchDispatcher::DidFail(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_NE(blink::ServiceWorkerStatusCode::kOk, status);
  Complete(status, FetchEventResult::kShouldFallback,
           blink::mojom::FetchAPIResponse::New(), nullptr /* body_as_stream */,
           nullptr /* timing */);
}

void ServiceWorkerFetchDispatcher::DidFinish(
    int request_id,
    FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  net_log_.EndEvent(net::NetLogEventType::SERVICE_WORKER_FETCH_EVENT);
  Complete(blink::ServiceWorkerStatusCode::kOk, fetch_result,
           std::move(response), std::move(body_as_stream), std::move(timing));
}

void ServiceWorkerFetchDispatcher::Complete(
    blink::ServiceWorkerStatusCode status,
    FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing) {
  DCHECK(fetch_callback_);

  did_complete_ = true;
  net_log_.EndEvent(
      net::NetLogEventType::SERVICE_WORKER_DISPATCH_FETCH_EVENT,
      base::Bind(&NetLogFetchEventCallback, status, fetch_result));

  std::move(fetch_callback_)
      .Run(status, fetch_result, std::move(response), std::move(body_as_stream),
           std::move(timing), version_);
}

// Non-S13nServiceWorker
bool ServiceWorkerFetchDispatcher::MaybeStartNavigationPreload(
    net::URLRequest* original_request,
    base::OnceClosure on_response) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return false;
  }
  if (!version_->navigation_preload_state().enabled)
    return false;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (request_body_blob_)
    return false;

  ResourceRequestInfoImpl* original_info =
      ResourceRequestInfoImpl::ForRequest(original_request);
  ResourceRequesterInfo* requester_info = original_info->requester_info();
  DCHECK(requester_info->IsBrowserSideNavigation());
  auto url_loader_factory = std::make_unique<URLLoaderFactoryImpl>(
      ResourceRequesterInfo::CreateForNavigationPreload(requester_info));

  network::ResourceRequest request;
  request.method = original_request->method();
  request.url = original_request->url();
  // TODO(horo): Set site_for_cookies to support Same-site Cookies.
  request.request_initiator =
      original_request->initiator().has_value()
          ? original_request->initiator()
          : url::Origin::Create(original_request->url());
  request.referrer = GURL(original_request->referrer());
  request.referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(original_info->GetReferrerPolicy());
  request.is_prerendering = original_info->IsPrerendering();
  request.load_flags = original_request->load_flags();
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  request.priority = original_request->priority();
  request.skip_service_worker = true;
  request.do_not_prompt_for_login = true;
  request.render_frame_id = original_info->GetRenderFrameID();
  request.is_main_frame = original_info->IsMainFrame();
  request.enable_load_timing = original_info->is_load_timing_enabled();
  request.report_raw_headers = original_info->ShouldReportRawHeaders();
  request.throttling_profile_id =
      network::ThrottlingController::GetProfileIDForNetLogSource(
          original_request->net_log().source().id);

  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
      version_->navigation_preload_state().header.length());
  request.headers.CopyFrom(original_request->extra_request_headers());
  request.headers.SetHeader("Service-Worker-Navigation-Preload",
                            version_->navigation_preload_state().header);

  const int request_id = ResourceDispatcherHostImpl::Get()->MakeRequestID();
  DCHECK_LT(request_id, -1);

  preload_handle_ = blink::mojom::FetchEventPreloadHandle::New();
  network::mojom::URLLoaderClientPtr inner_url_loader_client;
  preload_handle_->url_loader_client_request =
      mojo::MakeRequest(&inner_url_loader_client);
  auto url_loader_client = std::make_unique<DelegatingURLLoaderClient>(
      std::move(inner_url_loader_client), std::move(on_response), request);
  network::mojom::URLLoaderClientPtr url_loader_client_to_pass;
  url_loader_client->Bind(&url_loader_client_to_pass);
  network::mojom::URLLoaderPtr url_loader;

  url_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader), original_info->GetRouteID(), request_id,
      network::mojom::kURLLoadOptionNone, request,
      std::move(url_loader_client_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          original_request->traffic_annotation()));

  preload_handle_->url_loader = url_loader.PassInterface();

  DCHECK(!url_loader_assets_);
  url_loader_assets_ = base::MakeRefCounted<URLLoaderAssets>(
      std::move(url_loader_factory), std::move(url_loader_client));
  return true;
}

// S13nServiceWorker
bool ServiceWorkerFetchDispatcher::MaybeStartNavigationPreloadWithURLLoader(
    const network::ResourceRequest& original_request,
    URLLoaderFactoryGetter* url_loader_factory_getter,
    base::OnceClosure on_response) {
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME &&
      resource_type_ != RESOURCE_TYPE_SUB_FRAME) {
    return false;
  }
  if (!version_->navigation_preload_state().enabled)
    return false;
  // TODO(horo): Currently NavigationPreload doesn't support request body.
  if (request_->request_body)
    return false;

  network::ResourceRequest resource_request(original_request);
  // Set to SUB_RESOURCE because we shouldn't trigger NavigationResourceThrottle
  // for the service worker navigation preload request.
  resource_request.resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  resource_request.skip_service_worker = true;
  resource_request.do_not_prompt_for_login = true;

  DCHECK(net::HttpUtil::IsValidHeaderValue(
      version_->navigation_preload_state().header));
  ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
      version_->navigation_preload_state().header.length());
  resource_request.headers.SetHeader(
      "Service-Worker-Navigation-Preload",
      version_->navigation_preload_state().header);

  preload_handle_ = blink::mojom::FetchEventPreloadHandle::New();

  // Create the DelegatingURLLoaderClient, which becomes the
  // URLLoaderClient for the navigation preload network request.
  network::mojom::URLLoaderClientPtr inner_url_loader_client;
  preload_handle_->url_loader_client_request =
      mojo::MakeRequest(&inner_url_loader_client);
  auto url_loader_client = std::make_unique<DelegatingURLLoaderClient>(
      std::move(inner_url_loader_client), std::move(on_response),
      resource_request);

  // Start the network request for the URL using the network loader.
  // TODO(falken): What to do about routing_id, request_id?
  network::mojom::URLLoaderClientPtr url_loader_client_to_pass;
  url_loader_client->Bind(&url_loader_client_to_pass);
  network::mojom::URLLoaderPtr url_loader;
  url_loader_factory_getter->GetNetworkFactory()->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader), -1 /* routing_id? */,
      -1 /* request_id? */, network::mojom::kURLLoadOptionNone,
      resource_request, std::move(url_loader_client_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          kNavigationPreloadTrafficAnnotation));

  preload_handle_->url_loader = url_loader.PassInterface();

  DCHECK(!url_loader_assets_);
  // Unlike the non-S13N code path, we don't own the URLLoaderFactory being used
  // (it's the generic network factory), so we don't need to pass it to
  // URLLoaderAssets to keep it alive.
  url_loader_assets_ = base::MakeRefCounted<URLLoaderAssets>(
      nullptr /* url_loader_factory */, std::move(url_loader_client));
  return true;
}

ServiceWorkerMetrics::EventType ServiceWorkerFetchDispatcher::GetEventType()
    const {
  return ResourceTypeToEventType(resource_type_);
}

// static
void ServiceWorkerFetchDispatcher::OnFetchEventFinished(
    ServiceWorkerVersion* version,
    int event_finish_id,
    scoped_refptr<URLLoaderAssets> url_loader_assets,
    blink::mojom::ServiceWorkerEventStatus status,
    base::TimeTicks dispatch_event_time) {
  version->FinishRequest(
      event_finish_id,
      status != blink::mojom::ServiceWorkerEventStatus::ABORTED,
      dispatch_event_time);
}

}  // namespace content
