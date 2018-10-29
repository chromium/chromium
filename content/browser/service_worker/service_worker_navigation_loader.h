// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_response_type.h"
#include "content/browser/service_worker/service_worker_url_job_wrapper.h"
#include "content/browser/url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class ServiceWorkerVersion;

// S13nServiceWorker:
// ServiceWorkerNavigationLoader is the URLLoader used for main resource
// requests (i.e., navigation and shared worker requests) that (potentially) go
// through a service worker. This loader is only used for the main resource
// request; once the response is delivered, the resulting client loads
// subresources via ServiceWorkerSubresourceLoader.
//
// This class works similarly to ServiceWorkerURLRequestJob but with
// network::mojom::URLLoader instead of URLRequest.
//
// This class is owned by the job wrapper until it is bound to a URLLoader
// request. After it is bound |this| is kept alive until the Mojo connection to
// this URLLoader is dropped.
class CONTENT_EXPORT ServiceWorkerNavigationLoader
    : public network::mojom::URLLoader {
 public:
  using Delegate = ServiceWorkerURLJobWrapper::Delegate;
  using ResponseType = ServiceWorkerResponseType;

  // Created by ServiceWorkerControlleeRequestHandler::MaybeCreateLoader
  // when starting to load a main resource.
  //
  // For the navigation case, this job typically works in the following order:
  // 1. One of the FallbackTo* or ForwardTo* methods are called via
  //    URLJobWrapper by ServiceWorkerControlleeRequestHandler, which
  //    determines how the request should be served (e.g. should fallback
  //    to network or should be sent to the SW). If it decides to fallback
  //    to the network this will call |loader_callback| with a null
  //    RequestHandler, which will be then handled by NavigationURLLoaderImpl.
  // 2. If it is decided that the request should be sent to the SW,
  //    this job calls |loader_callback|, passing StartRequest as the
  //    RequestHandler.
  // 3. At this point, the NavigationURLLoaderImpl can throttle the request,
  //    and invoke the RequestHandler later with a possibly modified request.
  // 4. StartRequest is invoked. This dispatches a FetchEvent.
  // 5. DidDispatchFetchEvent() determines the request's final destination. If
  //    it turns out we need to fallback to network, it calls
  //    |fallback_callback|.
  // 6. Otherwise if the SW returned a stream or blob as a response
  //    this job passes the response to the network::mojom::URLLoaderClientPtr
  //    connected to NavigationURLLoaderImpl (for resource loading for
  //    navigation), that was given to StartRequest. This forwards the
  //    blob/stream data pipe to the NavigationURLLoader.
  //
  // Loads for shared workers work similarly, except SharedWorkerScriptLoader
  // is used instead of NavigationURLLoaderImpl.
  ServiceWorkerNavigationLoader(
      NavigationLoaderInterceptor::LoaderCallback loader_callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback,
      Delegate* delegate,
      const network::ResourceRequest& tentative_resource_request,
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter);

  ~ServiceWorkerNavigationLoader() override;

  // Called via URLJobWrapper.
  void FallbackToNetwork();
  void ForwardToServiceWorker();
  bool ShouldFallbackToNetwork();
  bool ShouldForwardToServiceWorker();

  // The navigation request that was holding this job is
  // going away. Calling this internally calls |DeleteIfNeeded()|
  // and may delete |this| if it is not bound to a endpoint.
  // Otherwise |this| will be kept around as far as the loader
  // endpoint is held by the client.
  void DetachedFromRequest();

  base::WeakPtr<ServiceWorkerNavigationLoader> AsWeakPtr();

 private:
  class StreamWaiter;
  enum class Status {
    kNotStarted,
    // |binding_| is bound and the fetch event is being dispatched to the
    // service worker.
    kStarted,
    // The response head has been sent to |url_loader_client_|. The response
    // body is being streamed.
    kSentHeader,
    // OnComplete() was called on |url_loader_client_|, or fallback to network
    // occurred so the request was not handled.
    kCompleted,
  };

  // For FORWARD_TO_SERVICE_WORKER case.
  void StartRequest(const network::ResourceRequest& resource_request,
                    network::mojom::URLLoaderRequest request,
                    network::mojom::URLLoaderClientPtr client);
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

  // Calls url_loader_client_->OnReceiveResponse() with |response_head_|.
  void CommitResponseHeaders();
  // Calls url_loader_client_->OnComplete().
  void CommitCompleted(int error_code);

  // network::mojom::URLLoader:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBlobReadingComplete(int net_error);

  void OnConnectionClosed();
  void DeleteIfNeeded();

  void ReportDestination(
      ServiceWorkerMetrics::MainResourceRequestDestination destination);

  // Records loading milestones. Called only after ForwardToServiceWorker() is
  // called and there was no error. |handled| is true when a fetch handler
  // handled the request (i.e. non network fallback case).
  void RecordTimingMetrics(bool handled);

  void TransitionToStatus(Status new_status);

  ResponseType response_type_ = ResponseType::NOT_DETERMINED;
  NavigationLoaderInterceptor::LoaderCallback loader_callback_;
  NavigationLoaderInterceptor::FallbackCallback fallback_callback_;

  // |delegate_| is non-null and owns |this| until DetachedFromRequest() is
  // called. Once that is called, |delegate_| is reset to null and |this| owns
  // itself, self-destructing when a connection error on |binding_| occurs.
  //
  // Note: A WeakPtr wouldn't be super safe here because the delegate can
  // conceivably still be alive and used for another loader, after calling
  // DetachedFromRequest() for this loader.
  Delegate* delegate_ = nullptr;

  network::ResourceRequest resource_request_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  std::unique_ptr<ServiceWorkerFetchDispatcher> fetch_dispatcher_;
  std::unique_ptr<StreamWaiter> stream_waiter_;
  // The blob needs to be held while it's read to keep it alive.
  blink::mojom::BlobPtr body_as_blob_;

  bool did_navigation_preload_ = false;
  network::ResourceResponseHead response_head_;

  bool devtools_attached_ = false;
  blink::mojom::ServiceWorkerFetchEventTimingPtr fetch_event_timing_;
  base::TimeTicks completion_time_;

  // Pointer to the URLLoaderClient (i.e. NavigationURLLoader).
  network::mojom::URLLoaderClientPtr url_loader_client_;
  mojo::Binding<network::mojom::URLLoader> binding_;

  Status status_ = Status::kNotStarted;

  base::WeakPtrFactory<ServiceWorkerNavigationLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_H_
