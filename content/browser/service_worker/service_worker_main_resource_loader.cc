// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/fetch/fetch_request_type_converters.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"

namespace content {

namespace {

std::string ComposeFetchEventResultString(
    ServiceWorkerFetchDispatcher::FetchEventResult result,
    const blink::mojom::FetchAPIResponse& response) {
  if (result == ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback)
    return "Fallback to network";
  std::stringstream stream;
  stream << "Got response (status_code: " << response.status_code
         << " status_text: '" << response.status_text << "')";
  return stream.str();
}

}  // namespace

// This class waits for completion of a stream response from the service worker.
// It calls ServiceWorkerMainResourceLoader::CommitCompleted() upon completion
// of the response.
class ServiceWorkerMainResourceLoader::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(ServiceWorkerMainResourceLoader* owner,
               mojo::PendingReceiver<blink::mojom::ServiceWorkerStreamCallback>
                   callback_receiver)
      : owner_(owner), receiver_(this, std::move(callback_receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }

  StreamWaiter(const StreamWaiter&) = delete;
  StreamWaiter& operator=(const StreamWaiter&) = delete;

  // Implements mojom::ServiceWorkerStreamCallback.
  void OnCompleted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::OK, "Stream has completed.");
  }
  void OnAborted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::ERR_ABORTED, "Stream has aborted.");
  }

 private:
  raw_ptr<ServiceWorkerMainResourceLoader> owner_;
  mojo::Receiver<blink::mojom::ServiceWorkerStreamCallback> receiver_;
};

ServiceWorkerMainResourceLoader::ServiceWorkerMainResourceLoader(
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    base::WeakPtr<ServiceWorkerContainerHost> container_host,
    int frame_tree_node_id)
    : fallback_callback_(std::move(fallback_callback)),
      container_host_(std::move(container_host)),
      frame_tree_node_id_(frame_tree_node_id) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerMainResourceLoader::ServiceWorkerMainResourceLoader", this,
      TRACE_EVENT_FLAG_FLOW_OUT);

  response_head_->load_timing.request_start = base::TimeTicks::Now();
  response_head_->load_timing.request_start_time = base::Time::Now();
}

ServiceWorkerMainResourceLoader::~ServiceWorkerMainResourceLoader() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerMainResourceLoader::~ServiceWorkerMainResourceLoader", this,
      TRACE_EVENT_FLAG_FLOW_IN);
}

void ServiceWorkerMainResourceLoader::DetachedFromRequest() {
  is_detached_ = true;
  // Clear |fallback_callback_| since it's no longer safe to invoke it because
  // the bound object has been destroyed.
  fallback_callback_.Reset();
  DeleteIfNeeded();
}

base::WeakPtr<ServiceWorkerMainResourceLoader>
ServiceWorkerMainResourceLoader::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerMainResourceLoader::StartRequest(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerMainResourceLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", resource_request.url.spec());
  DCHECK(blink::ServiceWorkerLoaderHelpers::IsMainRequestDestination(
      resource_request.destination));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  resource_request_ = resource_request;
  if (container_host_ && container_host_->fetch_request_window_id()) {
    resource_request_.fetch_window_id =
        absl::make_optional(container_host_->fetch_request_window_id());
  }

  DCHECK(!receiver_.is_bound());
  DCHECK(!url_loader_client_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerMainResourceLoader::OnConnectionClosed,
                     base::Unretained(this)));
  url_loader_client_.Bind(std::move(client));

  TransitionToStatus(Status::kStarted);

  if (!container_host_) {
    // We lost |container_host_| (for the client) somehow before dispatching
    // FetchEvent.
    CommitCompleted(net::ERR_ABORTED, "No container host");
    return;
  }

  scoped_refptr<ServiceWorkerVersion> active_worker =
      container_host_->controller();
  if (!active_worker) {
    CommitCompleted(net::ERR_FAILED, "No active worker");
    return;
  }

  base::WeakPtr<ServiceWorkerContextCore> core = active_worker->context();
  if (!core) {
    CommitCompleted(net::ERR_ABORTED, "No service worker context");
    return;
  }
  scoped_refptr<ServiceWorkerContextWrapper> context = core->wrapper();
  DCHECK(context);

  // Dispatch the fetch event.
  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      blink::mojom::FetchAPIRequest::From(resource_request_),
      resource_request_.destination, container_host_->client_uuid(),
      active_worker,
      base::BindOnce(&ServiceWorkerMainResourceLoader::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr(), active_worker,
                     active_worker->running_status()),
      base::BindOnce(&ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr()),
      /*is_offline_capability_check=*/false);

  if (container_host_->IsContainerForWindowClient()) {
    did_navigation_preload_ = fetch_dispatcher_->MaybeStartNavigationPreload(
        resource_request_, std::move(context), frame_tree_node_id_);
  }

  // Record worker start time here as |fetch_dispatcher_| will start a service
  // worker if there is no running service worker.
  response_head_->load_timing.service_worker_start_time =
      base::TimeTicks::Now();
  fetch_dispatcher_->Run();
}

void ServiceWorkerMainResourceLoader::CommitResponseHeaders() {
  DCHECK(url_loader_client_.is_bound());
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::CommitResponseHeaders",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "response_code", response_head_->headers->response_code(), "status_text",
      response_head_->headers->GetStatusText());
  TransitionToStatus(Status::kSentHeader);
}

void ServiceWorkerMainResourceLoader::CommitResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  TransitionToStatus(Status::kSentBody);
  url_loader_client_->OnReceiveResponse(response_head_.Clone(),
                                        std::move(response_body),
                                        std::move(cached_metadata));
}

void ServiceWorkerMainResourceLoader::CommitEmptyResponseAndComplete() {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (CreateDataPipe(nullptr, producer_handle, consumer_handle) !=
      MOJO_RESULT_OK) {
    CommitCompleted(net::ERR_INSUFFICIENT_RESOURCES,
                    "Can't create empty data pipe");
    return;
  }

  producer_handle.reset();  // The data pipe is empty.
  CommitResponseBody(std::move(consumer_handle), absl::nullopt);
  CommitCompleted(net::OK, "No body exists.");
}

void ServiceWorkerMainResourceLoader::CommitCompleted(int error_code,
                                                      const char* reason) {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::CommitCompleted", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      net::ErrorToString(error_code), "reason", TRACE_STR_COPY(reason));

  DCHECK(url_loader_client_.is_bound());
  TransitionToStatus(Status::kCompleted);
  if (error_code == net::OK)
    RecordTimingMetrics(true);

  // |stream_waiter_| calls this when done.
  stream_waiter_.reset();

  url_loader_client_->OnComplete(
      network::URLLoaderCompletionStatus(error_code));
}

void ServiceWorkerMainResourceLoader::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version,
    EmbeddedWorkerStatus initial_worker_status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::DidPrepareFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "initial_worker_status",
      EmbeddedWorkerInstance::StatusToString(initial_worker_status));

  devtools_attached_ = version->embedded_worker()->devtools_attached();
}

void ServiceWorkerMainResourceLoader::DidDispatchFetchEvent(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(status_, Status::kStarted);

  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::DidDispatchFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
      blink::ServiceWorkerStatusToString(status), "result",
      ComposeFetchEventResultString(fetch_result, *response));

  ServiceWorkerMetrics::RecordFetchEventStatus(true /* is_main_resource */,
                                               status);
  if (!container_host_) {
    // The navigation or shared worker startup is cancelled. Just abort.
    CommitCompleted(net::ERR_ABORTED, "No container host");
    return;
  }

  fetch_event_timing_ = std::move(timing);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // Dispatching the event to the service worker failed. Do a last resort
    // attempt to load the page via network as if there was no service worker.
    // It'd be more correct and simpler to remove this path and show an error
    // page, but the risk is that the user will be stuck if there's a persistent
    // failure.
    container_host_->NotifyControllerLost();
    if (fallback_callback_) {
      std::move(fallback_callback_)
          .Run(true /* reset_subresource_loader_params */);
    }
    return;
  }

  // Record the timing of when the fetch event is dispatched on the worker
  // thread. This is used for PerformanceResourceTiming#fetchStart and
  // PerformanceResourceTiming#requestStart, but it's still under spec
  // discussion.
  // See https://github.com/w3c/resource-timing/issues/119 for more details.
  // Exposed as PerformanceResourceTiming#fetchStart.
  response_head_->load_timing.service_worker_ready_time =
      fetch_event_timing_->dispatch_event_time;
  // Exposed as PerformanceResourceTiming#requestStart.
  response_head_->load_timing.send_start =
      fetch_event_timing_->dispatch_event_time;
  // Recorded for the DevTools.
  response_head_->load_timing.send_end =
      fetch_event_timing_->dispatch_event_time;

  // Records the metrics only if the code has been executed successfully in
  // the service workers because we aim to see the fallback ratio and timing.
  RecordFetchEventHandlerMetrics(fetch_result);

  if (fetch_result ==
      ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback) {
    TransitionToStatus(Status::kCompleted);
    RecordTimingMetrics(false);
    // TODO(falken): Propagate the timing info to the renderer somehow, or else
    // Navigation Timing etc APIs won't know about service worker.
    if (fallback_callback_) {
      std::move(fallback_callback_)
          .Run(false /* reset_subresource_loader_params */);
    }
    return;
  }

  DCHECK_EQ(fetch_result,
            ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse);

  // A response with status code 0 is Blink telling us to respond with
  // network error.
  if (response->status_code == 0) {
    // TODO(falken): Use more specific errors. Or just add ERR_SERVICE_WORKER?
    CommitCompleted(net::ERR_FAILED, "Zero response status");
    return;
  }

  StartResponse(std::move(response), std::move(version),
                std::move(body_as_stream));
}

void ServiceWorkerMainResourceLoader::StartResponse(
    blink::mojom::FetchAPIResponsePtr response,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(status_, Status::kStarted);

  blink::ServiceWorkerLoaderHelpers::SaveResponseInfo(*response,
                                                      response_head_.get());

  response_head_->did_service_worker_navigation_preload =
      did_navigation_preload_;
  response_head_->load_timing.receive_headers_start = base::TimeTicks::Now();
  response_head_->load_timing.receive_headers_end =
      response_head_->load_timing.receive_headers_start;
  response_source_ = response->response_source;
  response_head_->load_timing.service_worker_fetch_start =
      fetch_event_timing_->dispatch_event_time;
  response_head_->load_timing.service_worker_respond_with_settled =
      fetch_event_timing_->respond_with_settled_time;

  if (resource_request_.request_initiator &&
      (resource_request_.request_initiator->IsSameOriginWith(
           resource_request_.url) ||
       network::TimingAllowOriginCheck(
           response_head_->parsed_headers->timing_allow_origin,
           *resource_request_.request_initiator))) {
    response_head_->timing_allow_passed = true;
  }

  // Make the navigated page inherit the SSLInfo from its controller service
  // worker's script. This affects the HTTPS padlock, etc, shown by the
  // browser. See https://crbug.com/392409 for details about this design.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  DCHECK(version->GetMainScriptResponse());
  response_head_->ssl_info = version->GetMainScriptResponse()->ssl_info;

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  absl::optional<net::RedirectInfo> redirect_info =
      blink::ServiceWorkerLoaderHelpers::ComputeRedirectInfo(resource_request_,
                                                             *response_head_);
  if (redirect_info) {
    TRACE_EVENT_WITH_FLOW2(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "redirect", "redirect url", redirect_info->new_url.spec());

    response_head_->encoded_data_length = 0;
    url_loader_client_->OnReceiveRedirect(*redirect_info,
                                          response_head_.Clone());
    // Our client is the navigation loader, which will start a new URLLoader for
    // the redirect rather than calling FollowRedirect(), so we're done here.
    TransitionToStatus(Status::kCompleted);
    return;
  }

  // We have a non-redirect response. Send the headers to the client.
  CommitResponseHeaders();

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "stream response");
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(body_as_stream->callback_receiver));
    CommitResponseBody(std::move(body_as_stream->stream), absl::nullopt);
    // StreamWaiter will call CommitCompleted() when done.
    return;
  }

  // Handle a blob response body.
  if (response->blob) {
    DCHECK(response->blob->blob.is_valid());
    body_as_blob_.Bind(std::move(response->blob->blob));
    mojo::ScopedDataPipeConsumerHandle data_pipe;
    int error = blink::ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
        &body_as_blob_, response->blob->size,
        base::BindOnce(&ServiceWorkerMainResourceLoader::OnBlobReadingComplete,
                       weak_factory_.GetWeakPtr()),
        &data_pipe);
    if (error != net::OK) {
      CommitCompleted(error, "Failed to read blob body");
      return;
    }
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "blob response");

    CommitResponseBody(std::move(data_pipe), absl::nullopt);
    // We continue in OnBlobReadingComplete().
    return;
  }

  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerMainResourceLoader::StartResponse", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "result", "no body");

  CommitEmptyResponseAndComplete();
}

// URLLoader implementation----------------------------------------

void ServiceWorkerMainResourceLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerMainResourceLoader::ResumeReadingBodyFromNet() {}

void ServiceWorkerMainResourceLoader::OnBlobReadingComplete(int net_error) {
  CommitCompleted(net_error, "Blob has been read.");
  body_as_blob_.reset();
}

void ServiceWorkerMainResourceLoader::OnConnectionClosed() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::OnConnectionClosed",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // The fetch dispatcher or stream waiter may still be running. Don't let them
  // do callbacks back to this loader, since it is now done with the request.
  // TODO(falken): Try to move this to CommitCompleted(), since the same
  // justification applies there too.
  weak_factory_.InvalidateWeakPtrs();
  fetch_dispatcher_.reset();
  stream_waiter_.reset();
  receiver_.reset();

  // Respond to the request if it's not yet responded to.
  if (status_ != Status::kCompleted)
    CommitCompleted(net::ERR_ABORTED, "Disconnected pipe before completed");

  url_loader_client_.reset();
  DeleteIfNeeded();
}

void ServiceWorkerMainResourceLoader::DeleteIfNeeded() {
  if (!receiver_.is_bound() && is_detached_)
    delete this;
}

void ServiceWorkerMainResourceLoader::RecordTimingMetrics(bool handled) {
  DCHECK(fetch_event_timing_);
  DCHECK(!completion_time_.is_null());

  // We only record these metrics for top-level navigation.
  if (resource_request_.destination !=
      network::mojom::RequestDestination::kDocument)
    return;

  // |fetch_event_timing_| is recorded in renderer so we can get reasonable
  // metrics only when TimeTicks are consistent across processes.
  if (!base::TimeTicks::IsHighResolution() ||
      !base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  // Don't record metrics when DevTools is attached to reduce noise.
  if (devtools_attached_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "ServiceWorker", "ServiceWorker.LoadTiming.MainFrame.MainResource", this,
      response_head_->load_timing.request_start, "url", resource_request_.url);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "ServiceWorker.LoadTiming.MainFrame.MainResource", this,
      completion_time_);

  // Time between the request is made and the request is routed to this loader.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      response_head_->load_timing.service_worker_start_time -
          response_head_->load_timing.request_start);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "StartToForwardServiceWorker", this,
      response_head_->load_timing.request_start);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "StartToForwardServiceWorker", this,
      response_head_->load_timing.service_worker_start_time);

  // Time spent for service worker startup.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ForwardServiceWorkerToWorkerReady2",
      response_head_->load_timing.service_worker_ready_time -
          response_head_->load_timing.service_worker_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "ForwardServiceWorkerToWorkerReady", this,
      response_head_->load_timing.service_worker_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "ForwardServiceWorkerToWorkerReady", this,
      response_head_->load_timing.service_worker_ready_time);

  // Browser -> Renderer IPC delay.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "WorkerReadyToFetchHandlerStart",
      fetch_event_timing_->dispatch_event_time -
          response_head_->load_timing.service_worker_ready_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "WorkerReadyToFetchHandlerStart", this,
      response_head_->load_timing.service_worker_ready_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "WorkerReadyToFetchHandlerStart", this,
      fetch_event_timing_->dispatch_event_time);

  // Time spent by fetch handlers.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "FetchHandlerStartToFetchHandlerEnd",
      fetch_event_timing_->respond_with_settled_time -
          fetch_event_timing_->dispatch_event_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerStartToFetchHandlerEnd", this,
      fetch_event_timing_->dispatch_event_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerStartToFetchHandlerEnd", this,
      fetch_event_timing_->respond_with_settled_time);

  if (handled) {
    // Renderer -> Browser IPC delay.
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "FetchHandlerEndToResponseReceived",
        response_head_->load_timing.receive_headers_end -
            fetch_event_timing_->respond_with_settled_time);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "ServiceWorker", "FetchHandlerEndToResponseReceived", this,
        fetch_event_timing_->respond_with_settled_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "ServiceWorker", "FetchHandlerEndToResponseReceived", this,
        response_head_->load_timing.receive_headers_end);

    // Time spent reading response body.
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "ResponseReceivedToCompleted2",
        completion_time_ - response_head_->load_timing.receive_headers_end);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "ServiceWorker", "ResponseReceivedToCompleted", this,
        response_head_->load_timing.receive_headers_end,
        "fetch_response_source",
        blink::ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
            response_source_));
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "ServiceWorker", "ResponseReceivedToCompleted", this, completion_time_);
    // Same as above, breakdown by response source.
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"ServiceWorker.LoadTiming.MainFrame.MainResource."
             "ResponseReceivedToCompleted2.",
             blink::ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
                 response_source_)}),
        completion_time_ - response_head_->load_timing.receive_headers_end);
  } else {
    // Renderer -> Browser IPC delay (network fallback case).
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "FetchHandlerEndToFallbackNetwork",
        completion_time_ - fetch_event_timing_->respond_with_settled_time);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "ServiceWorker", "FetchHandlerEndToFallbackNetwork", this,
        fetch_event_timing_->respond_with_settled_time);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "ServiceWorker", "FetchHandlerEndToFallbackNetwork", this,
        completion_time_);
  }
}

void ServiceWorkerMainResourceLoader::RecordFetchEventHandlerMetrics(
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MainFrame.MainResource.FetchResult",
                            fetch_result);

  // Time spent by fetch handlers per |fetch_result|.
  base::UmaHistogramTimes(
      base::StrCat({
          "ServiceWorker.LoadTiming.MainFrame.MainResource."
          "FetchHandlerStartToFetchHandlerEndByFetchResult",
          ServiceWorkerFetchDispatcher::FetchEventResultToSuffix(fetch_result),
      }),
      fetch_event_timing_->respond_with_settled_time -
          fetch_event_timing_->dispatch_event_time);
}

void ServiceWorkerMainResourceLoader::TransitionToStatus(Status new_status) {
#if DCHECK_IS_ON()
  switch (new_status) {
    case Status::kNotStarted:
      NOTREACHED();
      break;
    case Status::kStarted:
      DCHECK_EQ(status_, Status::kNotStarted);
      break;
    case Status::kSentHeader:
      DCHECK_EQ(status_, Status::kStarted);
      break;
    case Status::kSentBody:
      DCHECK_EQ(status_, Status::kSentHeader);
      break;
    case Status::kCompleted:
      DCHECK(
          // Network fallback before interception.
          status_ == Status::kNotStarted ||
          // Network fallback after interception.
          status_ == Status::kStarted ||
          // Pipe creation failure for empty response.
          status_ == Status::kSentHeader ||
          // Success case or error while sending the response's body.
          status_ == Status::kSentBody);
      break;
  }
#endif  // DCHECK_IS_ON()

  status_ = new_status;
  if (new_status == Status::kCompleted)
    completion_time_ = base::TimeTicks::Now();
}

ServiceWorkerMainResourceLoaderWrapper::ServiceWorkerMainResourceLoaderWrapper(
    std::unique_ptr<ServiceWorkerMainResourceLoader> loader)
    : loader_(std::move(loader)) {}

ServiceWorkerMainResourceLoaderWrapper::
    ~ServiceWorkerMainResourceLoaderWrapper() {
  if (loader_)
    loader_.release()->DetachedFromRequest();
}

}  // namespace content
