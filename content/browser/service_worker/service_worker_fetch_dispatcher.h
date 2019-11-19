// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerVersion;
class URLLoaderFactoryGetter;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  // Indicates how the service worker handled a fetch event.
  enum class FetchEventResult {
    // Browser should fallback to native fetch.
    kShouldFallback,
    // Service worker provided a ServiceWorkerResponse.
    kGotResponse
  };

  using FetchCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode,
                              FetchEventResult,
                              blink::mojom::FetchAPIResponsePtr,
                              blink::mojom::ServiceWorkerStreamHandlePtr,
                              blink::mojom::ServiceWorkerFetchEventTimingPtr,
                              scoped_refptr<ServiceWorkerVersion>)>;
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;

  ServiceWorkerFetchDispatcher(blink::mojom::FetchAPIRequestPtr request,
                               ResourceType resource_type,
                               const std::string& client_id,
                               scoped_refptr<ServiceWorkerVersion> version,
                               base::OnceClosure prepare_callback,
                               FetchCallback fetch_callback);
  ~ServiceWorkerFetchDispatcher();

  // If appropriate, starts the navigation preload request and creates
  // |preload_handle_|. Returns true if it started navigation preload.
  bool MaybeStartNavigationPreload(
      const network::ResourceRequest& original_request,
      URLLoaderFactoryGetter* url_loader_factory_getter,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int frame_tree_node_id);

  // Dispatches a fetch event to the |version| given in ctor, and fires
  // |fetch_callback_| (also given in ctor) once a response is received from the
  // service worker. It runs |prepare_callback_| as an intermediate step once
  // the version is activated and running.
  void Run();

 private:
  class ResponseCallback;
  class URLLoaderAssets;

  void DidWaitForActivation();
  void StartWorker();
  void DidStartWorker(blink::ServiceWorkerStatusCode status);
  void DispatchFetchEvent();
  void DidFailToDispatch(std::unique_ptr<ResponseCallback> callback,
                         blink::ServiceWorkerStatusCode status);
  void DidFail(blink::ServiceWorkerStatusCode status);
  void DidFinish(int request_id,
                 FetchEventResult fetch_result,
                 blink::mojom::FetchAPIResponsePtr response,
                 blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                 blink::mojom::ServiceWorkerFetchEventTimingPtr timing);
  void Complete(blink::ServiceWorkerStatusCode status,
                FetchEventResult fetch_result,
                blink::mojom::FetchAPIResponsePtr response,
                blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                blink::mojom::ServiceWorkerFetchEventTimingPtr timing);

  // The fetch event stays open until all respondWith() and waitUntil() promises
  // are settled. This function is called once the renderer signals that
  // happened. |fetch_callback_| can run before this, once respondWith() is
  // settled.
  static void OnFetchEventFinished(
      ServiceWorkerVersion* version,
      int event_finish_id,
      scoped_refptr<URLLoaderAssets> url_loader_assets,
      blink::mojom::ServiceWorkerEventStatus status);

  ServiceWorkerMetrics::EventType GetEventType() const;

  blink::mojom::FetchAPIRequestPtr request_;
  std::string client_id_;
  scoped_refptr<ServiceWorkerVersion> version_;
  const ResourceType resource_type_;
  base::OnceClosure prepare_callback_;
  FetchCallback fetch_callback_;

  scoped_refptr<URLLoaderAssets> url_loader_assets_;

  // |preload_handle_| holds the URLLoader and URLLoaderClient for the service
  // worker to receive the navigation preload response. It's passed to the
  // service worker along with the fetch event.
  blink::mojom::FetchEventPreloadHandlePtr preload_handle_;

  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
