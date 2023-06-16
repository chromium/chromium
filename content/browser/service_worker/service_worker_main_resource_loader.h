// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class ServiceWorkerContainerHost;
class ServiceWorkerVersion;

// ServiceWorkerMainResourceLoader is the URLLoader used for main resource
// requests (i.e., navigation and shared worker requests) that go through a
// service worker. This loader is only used for the main resource request; once
// the response is delivered, the resulting client loads subresources via
// ServiceWorkerSubresourceLoader.
//
// This class is owned by ServiceWorkerControlleeRequestHandler until it is
// bound to a URLLoader request. After it is bound |this| is kept alive until
// the Mojo connection to this URLLoader is dropped.
class CONTENT_EXPORT ServiceWorkerMainResourceLoader
    : public network::mojom::URLLoader,
      public ServiceWorkerResourceLoader {
 public:
  // Created by ServiceWorkerControlleeRequestHandler
  // after it determines the load should go through a service worker.
  //
  // For the navigation case, this job typically works in the following order:
  // 1. ServiceWorkerControlleeRequestHandler::MaybeCreateLoader() creates the
  //    ServiceWorkerMainResourceLoader, passing StartRequest() as the
  //    RequestHandler.
  // 2. At this point, the NavigationURLLoaderImpl can throttle the request,
  //    and invoke the RequestHandler later with a possibly modified request.
  // 3. StartRequest is invoked. This dispatches a FetchEvent.
  // 4. DidDispatchFetchEvent() determines the request's final destination. If
  //    it turns out we need to fallback to network, it calls
  //    |fallback_callback|.
  // 5. Otherwise if the SW returned a stream or blob as a response
  //    this job passes the response to the network::mojom::URLLoaderClient
  //    connected to NavigationURLLoaderImpl (for resource loading for
  //    navigation), that was given to StartRequest. This forwards the
  //    blob/stream data pipe to the NavigationURLLoader.
  //
  // Loads for shared workers work similarly, except SharedWorkerScriptLoader
  // is used instead of NavigationURLLoaderImpl.
  ServiceWorkerMainResourceLoader(
      NavigationLoaderInterceptor::FallbackCallback fallback_callback,
      base::WeakPtr<ServiceWorkerContainerHost> container_host,
      int frame_tree_node_id);

  ServiceWorkerMainResourceLoader(const ServiceWorkerMainResourceLoader&) =
      delete;
  ServiceWorkerMainResourceLoader& operator=(
      const ServiceWorkerMainResourceLoader&) = delete;

  ~ServiceWorkerMainResourceLoader() override;

  // Passed as the RequestHandler for
  // NavigationLoaderInterceptor::MaybeCreateLoader.
  void StartRequest(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // The navigation request that was holding this job is
  // going away. Calling this internally calls |DeleteIfNeeded()|
  // and may delete |this| if it is not bound to a endpoint.
  // Otherwise |this| will be kept around as far as the loader
  // endpoint is held by the client.
  void DetachedFromRequest();

  base::WeakPtr<ServiceWorkerMainResourceLoader> AsWeakPtr();

 private:
  class StreamWaiter;
  class RaceNetworkRequestURLLoaderClient;
  enum class Status {
    kNotStarted,
    // |receiver_| is bound and the fetch event is being dispatched to the
    // service worker.
    kStarted,
    // The response head has been sent to |url_loader_client_|.
    kSentHeader,
    // The data pipe for the response body has been sent to
    // |url_loader_client_|. The body is being written to the pipe.
    kSentBody,
    // OnComplete() was called on |url_loader_client_|, or fallback to network
    // occurred so the request was not handled.
    kCompleted,
  };
  // Indicates what kind of preload request is dispatched before starting
  // the ServiceWorker.
  //
  // kNone: No preload request is triggered. This is the default state.
  // kRaceNetworkRequest:
  //    RaceNetworkRequest is triggered.
  //    TODO(crbug.com/1420517) This will be passed to the renderer and block
  //    the corresponding request from the ServiceWorker.
  // kNavigationPreload:
  //    Enabled when Navigation Preload is triggered.
  enum class DispatchedPreloadType {
    kNone,
    kRaceNetworkRequest,
    kNavigationPreload
  };

  void DidPrepareFetchEvent(scoped_refptr<ServiceWorkerVersion> version,
                            EmbeddedWorkerStatus initial_worker_status);
  void DidDispatchFetchEvent(
      blink::ServiceWorkerStatusCode status,
      ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
      blink::mojom::FetchAPIResponsePtr response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
      scoped_refptr<ServiceWorkerVersion> version);

  void StartResponse(blink::mojom::FetchAPIResponsePtr response,
                     scoped_refptr<ServiceWorkerVersion> version,
                     blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream);

  // ServiceWorkerResourceLoader overrides:
  // Calls url_loader_client_->OnReceiveResponse() with given |response_head|.
  void CommitResponseHeaders(
      const network::mojom::URLResponseHeadPtr& response_head) override;

  // Calls url_loader_client_->OnReceiveResponse() with
  // |response_head|, |response_body| and |cached_metadata|.
  void CommitResponseBody(
      const network::mojom::URLResponseHeadPtr& response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;

  // Creates and sends an empty response's body with the net::OK status.
  // Sends net::ERR_INSUFFICIENT_RESOURCES when it can't be created.
  void CommitEmptyResponseAndComplete() override;

  // Calls url_loader_client_->OnComplete(). |reason| will be recorded as an
  // argument of TRACE_EVENT.
  void CommitCompleted(int error_code, const char* reason) override;

  // Calls url_loader_client_->OnReceiveRedirect().
  void HandleRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHeadPtr& response_head) override;

  bool IsMainResourceLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBlobReadingComplete(int net_error);

  void OnConnectionClosed();
  void DeleteIfNeeded();

  // Records loading milestones. Called only after ForwardToServiceWorker() is
  // called and there was no error.
  bool InitRecordTimingMetricsIfEligible(
      const net::LoadTimingInfo& load_timing);
  // Called when the fetch handler handles the request.
  void RecordTimingMetricsForFetchHandlerHandledCase();
  // Called when the fetch handler doesn't handle the request (i.e. network
  // fallback case).
  void RecordTimingMetricsForNetworkFallbackCase();
  // Called when the response from RaceNetworkRequest is faster than the
  // response from the fetch handler.
  void RecordTimingMetricsForRaceNetworkRequestCase();
  // Time between the request is made and the request is routed to this loader.
  void RecordStartToForwardServiceWorkerTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Time spent for service worker startup.
  void RecordForwardServiceWorkerToWorkerReadyTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Browser -> Renderer IPC delay.
  void RecordWorkerReadyToFetchHandlerStartTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Time spent by fetch handlers.
  void RecordFetchHandlerStartToFetchHandlerEndTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Renderer -> Browser IPC delay.
  void RecordFetchHandlerEndToResponseReceivedTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Time spent reading response body.
  void RecordResponseReceivedToCompletedTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Time between the request is made and complete reading response body.
  void RecordStartToCompletedTiming(const net::LoadTimingInfo& load_timing,
                                    const std::string& initial_worker_status);
  // Time between the request is made and network fallback.
  void RecordStartToFallbackNetworkTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);
  // Renderer -> Browser IPC delay (network fallback case).
  void RecordFetchHandlerEndToFallbackNetworkTiming(
      const net::LoadTimingInfo& load_timing,
      const std::string& initial_worker_status);

  // Records metrics related to the fetch event handler execution.
  void RecordFetchEventHandlerMetrics(
      ServiceWorkerFetchDispatcher::FetchEventResult fetch_result);

  void TransitionToStatus(Status new_status);

  bool MaybeStartRaceNetworkRequest(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      scoped_refptr<ServiceWorkerVersion> version);

  NavigationLoaderInterceptor::FallbackCallback fallback_callback_;

  network::ResourceRequest resource_request_;

  base::WeakPtr<ServiceWorkerContainerHost> container_host_;
  const int frame_tree_node_id_;

  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::unique_ptr<StreamWaiter> stream_waiter_;
  // The blob needs to be held while it's read to keep it alive.
  mojo::Remote<blink::mojom::Blob> body_as_blob_;

  DispatchedPreloadType dispatched_preload_type_ = DispatchedPreloadType::kNone;

  network::mojom::URLResponseHeadPtr response_head_ =
      network::mojom::URLResponseHead::New();

  bool devtools_attached_ = false;
  blink::mojom::ServiceWorkerFetchEventTimingPtr fetch_event_timing_;
  base::TimeTicks completion_time_;
  network::mojom::FetchResponseSource response_source_ =
      network::mojom::FetchResponseSource::kUnspecified;

  // Pointer to the URLLoaderClient (i.e. NavigationURLLoader).
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  Status status_ = Status::kNotStarted;
  absl::optional<EmbeddedWorkerStatus> initial_embedded_worker_status_;
  bool is_detached_ = false;

  scoped_refptr<network::SharedURLLoaderFactory>
      race_network_request_url_loader_factory_;
  mojo::PendingRemote<network::mojom::URLLoader>
      race_network_request_url_loader_;
  std::unique_ptr<ServiceWorkerRaceNetworkRequestURLLoaderClient>
      race_network_request_loader_client_;
  std::unique_ptr<ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory>
      forwarded_race_network_request_url_loader_factory_;

  base::WeakPtrFactory<ServiceWorkerMainResourceLoader> weak_factory_{this};
};

// Owns a loader and calls DetachedFromRequest() to release it.
class ServiceWorkerMainResourceLoaderWrapper {
 public:
  explicit ServiceWorkerMainResourceLoaderWrapper(
      std::unique_ptr<ServiceWorkerMainResourceLoader> loader);

  ServiceWorkerMainResourceLoaderWrapper(
      const ServiceWorkerMainResourceLoaderWrapper&) = delete;
  ServiceWorkerMainResourceLoaderWrapper& operator=(
      const ServiceWorkerMainResourceLoaderWrapper&) = delete;

  ~ServiceWorkerMainResourceLoaderWrapper();

  ServiceWorkerMainResourceLoader* get() { return loader_.get(); }

 private:
  std::unique_ptr<ServiceWorkerMainResourceLoader> loader_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_H_
