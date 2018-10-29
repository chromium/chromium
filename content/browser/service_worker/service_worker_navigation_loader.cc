// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader.h"

#include <sstream>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/service_worker/service_worker_loader_helpers.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

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

bool BodyHasNoDataPipeGetters(const network::ResourceRequestBody* body) {
  if (!body)
    return true;
  for (const auto& elem : *body->elements()) {
    if (elem.type() == network::DataElement::TYPE_DATA_PIPE)
      return false;
  }
  return true;
}

}  // namespace

// This class waits for completion of a stream response from the service worker.
// It calls ServiceWorkerNavigationLoader::CommitCompleted() upon completion of
// the response.
class ServiceWorkerNavigationLoader::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(
      ServiceWorkerNavigationLoader* owner,
      scoped_refptr<ServiceWorkerVersion> streaming_version,
      blink::mojom::ServiceWorkerStreamCallbackRequest callback_request)
      : owner_(owner),
        streaming_version_(streaming_version),
        binding_(this, std::move(callback_request)) {
    streaming_version_->OnStreamResponseStarted();
    binding_.set_connection_error_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }
  ~StreamWaiter() override { streaming_version_->OnStreamResponseFinished(); }

  // Implements mojom::ServiceWorkerStreamCallback.
  void OnCompleted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::OK);
  }
  void OnAborted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::ERR_ABORTED);
  }

 private:
  ServiceWorkerNavigationLoader* owner_;
  scoped_refptr<ServiceWorkerVersion> streaming_version_;
  mojo::Binding<blink::mojom::ServiceWorkerStreamCallback> binding_;

  DISALLOW_COPY_AND_ASSIGN(StreamWaiter);
};

ServiceWorkerNavigationLoader::ServiceWorkerNavigationLoader(
    NavigationLoaderInterceptor::LoaderCallback callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    Delegate* delegate,
    const network::ResourceRequest& tentative_resource_request,
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter)
    : loader_callback_(std::move(callback)),
      fallback_callback_(std::move(fallback_callback)),
      delegate_(delegate),
      url_loader_factory_getter_(std::move(url_loader_factory_getter)),
      binding_(this),
      weak_factory_(this) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerNavigationLoader::ServiceWorkerNavigationloader", this,
      TRACE_EVENT_FLAG_FLOW_OUT, "url", tentative_resource_request.url.spec());

  DCHECK(delegate_);
  DCHECK(ServiceWorkerUtils::IsMainResourceType(
      static_cast<ResourceType>(tentative_resource_request.resource_type)));

  response_head_.load_timing.request_start = base::TimeTicks::Now();
  response_head_.load_timing.request_start_time = base::Time::Now();

  // Beware that the final resource request may change due to throttles, so
  // don't save |tentative_resource_request| here. We'll get the real one in
  // StartRequest.
}

ServiceWorkerNavigationLoader::~ServiceWorkerNavigationLoader() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerNavigationLoader::~ServiceWorkerNavigationloader", this,
      TRACE_EVENT_FLAG_FLOW_IN);
};

void ServiceWorkerNavigationLoader::FallbackToNetwork() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerNavigationLoader::FallbackToNetwork", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // The URLJobWrapper only calls this if this loader never intercepted the
  // request. Fallback to network after interception uses |fallback_callback_|
  // instead.
  DCHECK_EQ(status_, Status::kNotStarted);
  DCHECK_EQ(response_type_, ResponseType::NOT_DETERMINED);

  response_type_ = ResponseType::FALLBACK_TO_NETWORK;
  TransitionToStatus(Status::kCompleted);
  std::move(loader_callback_).Run({});
}

void ServiceWorkerNavigationLoader::ForwardToServiceWorker() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerNavigationLoader::ForwardToServiceWorker",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK_EQ(status_, Status::kNotStarted);
  DCHECK_EQ(response_type_, ResponseType::NOT_DETERMINED);

  response_type_ = ResponseType::FORWARD_TO_SERVICE_WORKER;

  std::move(loader_callback_)
      .Run(base::BindOnce(&ServiceWorkerNavigationLoader::StartRequest,
                          weak_factory_.GetWeakPtr()));
}

bool ServiceWorkerNavigationLoader::ShouldFallbackToNetwork() {
  return response_type_ == ResponseType::FALLBACK_TO_NETWORK;
}

bool ServiceWorkerNavigationLoader::ShouldForwardToServiceWorker() {
  return response_type_ == ResponseType::FORWARD_TO_SERVICE_WORKER;
}

void ServiceWorkerNavigationLoader::DetachedFromRequest() {
  delegate_ = nullptr;
  DeleteIfNeeded();
}

base::WeakPtr<ServiceWorkerNavigationLoader>
ServiceWorkerNavigationLoader::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerNavigationLoader::StartRequest(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  resource_request_ = resource_request;

  DCHECK(delegate_);
  DCHECK(!binding_.is_bound());
  DCHECK(!url_loader_client_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&ServiceWorkerNavigationLoader::OnConnectionClosed,
                     base::Unretained(this)));
  url_loader_client_ = std::move(client);

  DCHECK_EQ(ResponseType::FORWARD_TO_SERVICE_WORKER, response_type_);
  TransitionToStatus(Status::kStarted);

  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerNavigationLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);
  if (!active_worker) {
    delegate_->ReportDestination(
        ServiceWorkerMetrics::MainResourceRequestDestination::
            kErrorNoActiveWorkerFromDelegate);
    CommitCompleted(net::ERR_FAILED);
    return;
  }

  // ServiceWorkerFetchDispatcher requires a std::unique_ptr<ResourceRequest>
  // so make one here.
  // TODO(crbug.com/803125): Try to eliminate unnecessary copying?
  auto resource_request_to_pass =
      std::make_unique<network::ResourceRequest>(resource_request_);

  // Passing the request body over Mojo moves out the DataPipeGetter elements,
  // which would mean we should clone the body like
  // ServiceWorkerSubresourceLoader does. But we don't expect DataPipeGetters
  // here yet: they are only created by the renderer when converting from a
  // Blob, which doesn't happen for navigations. In interest of speed, just
  // don't clone until proven necessary.
  DCHECK(BodyHasNoDataPipeGetters(resource_request_to_pass->request_body.get()))
      << "We assumed there would be no data pipe getter elements here, but "
         "there are. Add code here to clone the body before proceeding.";

  // Dispatch the fetch event.
  delegate_->WillDispatchFetchEventForMainResource();
  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(resource_request_to_pass),
      std::string() /* request_body_blob_uuid */,
      0 /* request_body_blob_size */, nullptr /* request_body_blob */,
      std::string() /* client_id */, active_worker,
      net::NetLogWithSource() /* TODO(scottmg): net log? */,
      base::BindOnce(&ServiceWorkerNavigationLoader::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr(),
                     base::WrapRefCounted(active_worker),
                     active_worker->running_status()),
      base::BindOnce(&ServiceWorkerNavigationLoader::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr()));
  did_navigation_preload_ =
      fetch_dispatcher_->MaybeStartNavigationPreloadWithURLLoader(
          resource_request_, url_loader_factory_getter_.get(),
          base::DoNothing(/* TODO(crbug/762357): metrics? */));

  // Record worker start time here as |fetch_dispatcher_| will start a service
  // worker if there is no running service worker.
  response_head_.service_worker_start_time = base::TimeTicks::Now();
  fetch_dispatcher_->Run();
}

void ServiceWorkerNavigationLoader::CommitResponseHeaders() {
  DCHECK(url_loader_client_.is_bound());
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerNavigationLoader::CommitResponseHeaders",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "response_code", response_head_.headers->response_code(), "status_text",
      response_head_.headers->GetStatusText());
  TransitionToStatus(Status::kSentHeader);
  url_loader_client_->OnReceiveResponse(response_head_);
}

void ServiceWorkerNavigationLoader::CommitCompleted(int error_code) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerNavigationLoader::CommitCompleted", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "error_code", net::ErrorToString(error_code));

  DCHECK(url_loader_client_.is_bound());
  TransitionToStatus(Status::kCompleted);
  if (error_code == net::OK)
    RecordTimingMetrics(true);

  // |stream_waiter_| calls this when done.
  stream_waiter_.reset();

  url_loader_client_->OnComplete(
      network::URLLoaderCompletionStatus(error_code));
}

void ServiceWorkerNavigationLoader::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version,
    EmbeddedWorkerStatus initial_worker_status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerNavigationLoader::DidPrepareFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "initial_worker_status",
      EmbeddedWorkerInstance::StatusToString(initial_worker_status));

  // At this point a service worker is running and the fetch event is about
  // to dispatch. Record some load timings.
  base::TimeTicks now = base::TimeTicks::Now();
  response_head_.service_worker_ready_time = now;
  response_head_.load_timing.send_start = now;
  response_head_.load_timing.send_end = now;

  devtools_attached_ = version->embedded_worker()->devtools_attached();

  // Note that we don't record worker preparation time in S13nServiceWorker
  // path for now. If we want to measure worker preparation time we can
  // calculate it from response_head_.service_worker_ready_time and
  // response_head_.load_timing.request_start.
  // https://crbug.com/852664
  ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
      base::TimeDelta(), initial_worker_status,
      version->embedded_worker()->start_situation(), did_navigation_preload_,
      resource_request_.url);
}

void ServiceWorkerNavigationLoader::DidDispatchFetchEvent(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(status_, Status::kStarted);

  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerNavigationLoader::DidDispatchFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
      blink::ServiceWorkerStatusToString(status), "result",
      ComposeFetchEventResultString(fetch_result, *response));

  ServiceWorkerMetrics::RecordFetchEventStatus(true /* is_main_resource */,
                                               status);
  if (delegate_) {
    delegate_->ReportDestination(
        ServiceWorkerMetrics::MainResourceRequestDestination::kServiceWorker);
  }

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  if (!delegate_ || !delegate_->RequestStillValid(&result)) {
    // The navigation or shared worker startup is cancelled. Just abort.
    CommitCompleted(net::ERR_ABORTED);
    return;
  }

  fetch_event_timing_ = std::move(timing);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // Dispatching the event to the service worker failed. Do a last resort
    // attempt to load the page via network as if there was no service worker.
    // It'd be more correct and simpler to remove this path and show an error
    // page, but the risk is that the user will be stuck if there's a persistent
    // failure.
    delegate_->MainResourceLoadFailed();
    std::move(fallback_callback_)
        .Run(true /* reset_subresource_loader_params */);
    return;
  }

  if (fetch_result ==
      ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback) {
    TransitionToStatus(Status::kCompleted);
    RecordTimingMetrics(false);
    // TODO(falken): Propagate the timing info to the renderer somehow, or else
    // Navigation Timing etc APIs won't know about service worker.
    std::move(fallback_callback_)
        .Run(false /* reset_subresource_loader_params */);
    return;
  }

  DCHECK_EQ(fetch_result,
            ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse);

  // A response with status code 0 is Blink telling us to respond with
  // network error.
  if (response->status_code == 0) {
    CommitCompleted(net::ERR_FAILED);
    return;
  }

  StartResponse(std::move(response), std::move(version),
                std::move(body_as_stream));
}

void ServiceWorkerNavigationLoader::StartResponse(
    blink::mojom::FetchAPIResponsePtr response,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(status_, Status::kStarted);

  ServiceWorkerLoaderHelpers::SaveResponseInfo(*response, &response_head_);
  ServiceWorkerLoaderHelpers::SaveResponseHeaders(
      response->status_code, response->status_text, response->headers,
      &response_head_);

  response_head_.did_service_worker_navigation_preload =
      did_navigation_preload_;
  response_head_.load_timing.receive_headers_end = base::TimeTicks::Now();

  // Make the navigated page inherit the SSLInfo from its controller service
  // worker's script. This affects the HTTPS padlock, etc, shown by the
  // browser. See https://crbug.com/392409 for details about this design.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  response_head_.ssl_info = version->GetMainScriptHttpResponseInfo()->ssl_info;

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  base::Optional<net::RedirectInfo> redirect_info =
      ServiceWorkerLoaderHelpers::ComputeRedirectInfo(resource_request_,
                                                      response_head_);
  if (redirect_info) {
    TRACE_EVENT_WITH_FLOW2(
        "ServiceWorker", "ServiceWorkerNavigationLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "redirect", "redirect url", redirect_info->new_url.spec());

    response_head_.encoded_data_length = 0;
    url_loader_client_->OnReceiveRedirect(*redirect_info, response_head_);
    // Our client is the navigation loader, which will start a new URLLoader for
    // the redirect rather than calling FollowRedirect(), so we're done here.
    TransitionToStatus(Status::kCompleted);
    return;
  }

  // We have a non-redirect response. Send the headers to the client.
  CommitResponseHeaders();

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                           "ServiceWorkerNavigationLoader::StartResponse", this,
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "result", "stream response");
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(version), std::move(body_as_stream->callback_request));
    url_loader_client_->OnStartLoadingResponseBody(
        std::move(body_as_stream->stream));
    // StreamWaiter will call CommitCompleted() when done.
    return;
  }

  // Handle a blob response body.
  if (response->blob) {
    DCHECK(response->blob->blob.is_valid());
    body_as_blob_.Bind(std::move(response->blob->blob));
    mojo::ScopedDataPipeConsumerHandle data_pipe;
    int error = ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
        &body_as_blob_, response->blob->size,
        base::BindOnce(&ServiceWorkerNavigationLoader::OnBlobReadingComplete,
                       weak_factory_.GetWeakPtr()),
        &data_pipe);
    if (error != net::OK) {
      CommitCompleted(error);
      return;
    }
    TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                           "ServiceWorkerNavigationLoader::StartResponse", this,
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "result", "blob response");

    url_loader_client_->OnStartLoadingResponseBody(std::move(data_pipe));
    // We continue in OnBlobReadingComplete().
    return;
  }

  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerNavigationLoader::StartResponse", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "result", "no body");

  // The response has no body.
  CommitCompleted(net::OK);
}

// URLLoader implementation----------------------------------------

void ServiceWorkerNavigationLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  NOTIMPLEMENTED();
}

void ServiceWorkerNavigationLoader::ProceedWithResponse() {
  // ServiceWorkerNavigationLoader doesn't need to wait for
  // ProceedWithResponse() since it doesn't use MojoAsyncResourceHandler to load
  // the resource request.
}

void ServiceWorkerNavigationLoader::SetPriority(net::RequestPriority priority,
                                                int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void ServiceWorkerNavigationLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerNavigationLoader::ResumeReadingBodyFromNet() {}

void ServiceWorkerNavigationLoader::OnBlobReadingComplete(int net_error) {
  CommitCompleted(net_error);
  body_as_blob_.reset();
}

void ServiceWorkerNavigationLoader::OnConnectionClosed() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerNavigationLoader::OnConnectionClosed",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // The fetch dispatcher or stream waiter may still be running. Don't let them
  // do callbacks back to this loader, since it is now done with the request.
  // TODO(falken): Try to move this to CommitCompleted(), since the same
  // justification applies there too.
  weak_factory_.InvalidateWeakPtrs();
  fetch_dispatcher_.reset();
  stream_waiter_.reset();
  binding_.Close();

  // Respond to the request if it's not yet responded to.
  if (status_ != Status::kCompleted)
    CommitCompleted(net::ERR_ABORTED);

  url_loader_client_.reset();
  DeleteIfNeeded();
}

void ServiceWorkerNavigationLoader::DeleteIfNeeded() {
  if (!binding_.is_bound() && !delegate_)
    delete this;
}

void ServiceWorkerNavigationLoader::RecordTimingMetrics(bool handled) {
  DCHECK(fetch_event_timing_);
  DCHECK(!completion_time_.is_null());

  // We only record these metrics for top-level navigation.
  if (resource_request_.resource_type != RESOURCE_TYPE_MAIN_FRAME)
    return;

  // |fetch_event_timing_| is recorded in renderer so we can get reasonable
  // metrics only when TimeTicks are consistent across processes.
  if (!base::TimeTicks::IsHighResolution() ||
      !base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  // Don't record metrics when DevTools is attached to reduce noise.
  if (devtools_attached_)
    return;

  // Time between the request is made and the request is routed to this loader.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "StartToForwardServiceWorker",
      response_head_.service_worker_start_time -
          response_head_.load_timing.request_start);

  // Time spent for service worker startup.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "ForwardServiceWorkerToWorkerReady",
      response_head_.service_worker_ready_time -
          response_head_.service_worker_start_time);

  // Browser -> Renderer IPC delay.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "WorkerReadyToFetchHandlerStart",
      fetch_event_timing_->dispatch_event_time -
          response_head_.service_worker_ready_time);

  // Time spent by fetch handlers.
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "FetchHandlerStartToFetchHandlerEnd",
      fetch_event_timing_->respond_with_settled_time -
          fetch_event_timing_->dispatch_event_time);

  if (handled) {
    // Renderer -> Browser IPC delay.
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "FetchHandlerEndToResponseReceived",
        response_head_.load_timing.receive_headers_end -
            fetch_event_timing_->respond_with_settled_time);

    // Time spent reading response body.
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "ResponseReceivedToCompleted",
        completion_time_ - response_head_.load_timing.receive_headers_end);
  } else {
    // Renderer -> Browser IPC delay (network fallback case).
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.LoadTiming.MainFrame.MainResource."
        "FetchHandlerEndToFallbackNetwork",
        completion_time_ - fetch_event_timing_->respond_with_settled_time);
  }
}

void ServiceWorkerNavigationLoader::TransitionToStatus(Status new_status) {
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
    case Status::kCompleted:
      // kNotStarted -> kCompleted happens on network fallback before
      // interception.
      // kStarted -> kCompleted happens on error or network fallback after
      // interception.
      // kSentHeader -> kCompleted happens in the success case or error
      // while sending the body.
      DCHECK_NE(status_, Status::kCompleted);
      break;
  }
#endif  // DCHECK_IS_ON()

  status_ = new_status;
  if (new_status == Status::kCompleted)
    completion_time_ = base::TimeTicks::Now();
}

}  // namespace content
