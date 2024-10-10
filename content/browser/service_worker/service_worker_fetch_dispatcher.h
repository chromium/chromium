// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerVersion;

// A helper class to dispatch fetch event to a service worker.
class CONTENT_EXPORT ServiceWorkerFetchDispatcher {
 public:
  // Indicates how the service worker handled a fetch event.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchEventResult {
    // Browser should fallback to native fetch.
    kShouldFallback = 0,
    // Service worker provided a ServiceWorkerResponse.
    kGotResponse = 1,
    kMaxValue = kGotResponse,
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
                               network::mojom::RequestDestination destination,
                               const std::string& client_id,
                               const std::string& resulting_client_id,
                               scoped_refptr<ServiceWorkerVersion> version,
                               base::OnceClosure prepare_callback,
                               FetchCallback fetch_callback);

  ServiceWorkerFetchDispatcher(const ServiceWorkerFetchDispatcher&) = delete;
  ServiceWorkerFetchDispatcher& operator=(const ServiceWorkerFetchDispatcher&) =
      delete;

  ~ServiceWorkerFetchDispatcher();

  static const char* FetchEventResultToSuffix(FetchEventResult result);

  // If appropriate, starts the navigation preload request and creates
  // |preload_handle_|. Returns true if it started navigation preload.
  bool MaybeStartNavigationPreload(
      const network::ResourceRequest& original_request,
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      FrameTreeNodeId frame_tree_node_id);

  // Dispatches a fetch event to the |version| given in ctor, and fires
  // |fetch_callback_| (also given in ctor) once a response is received from the
  // service worker. It runs |prepare_callback_| as an intermediate step once
  // the version is activated and running.
  void Run();

  bool FetchCallbackIsNull() { return fetch_callback_.is_null(); }

  static scoped_refptr<network::SharedURLLoaderFactory>
  CreateNetworkURLLoaderFactory(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      FrameTreeNodeId frame_tree_node_id);

  static void ForceDisableHighPriorityFetchResponseCallbackForTesting(
      bool force_disable);

  void set_race_network_request_token(base::UnguessableToken token) {
    race_network_request_token_ = token;
  }
  void set_race_network_request_loader_factory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory) {
    race_network_request_loader_factory_ = std::move(factory);
  }

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
  void RunCallback(blink::ServiceWorkerStatusCode status,
                   FetchEventResult fetch_result,
                   blink::mojom::FetchAPIResponsePtr response,
                   blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
                   blink::mojom::ServiceWorkerFetchEventTimingPtr timing);

  // The fetch event stays open until all respondWith() and waitUntil() promises
  // are settled. This function is called once the renderer signals that
  // happened. |fetch_callback_| can run before this, once respondWith() is
  // settled.
  static void OnFetchEventFinished(
      base::WeakPtr<ServiceWorkerFetchDispatcher> fetch_dispatcher,
      ServiceWorkerVersion* version,
      int event_finish_id,
      scoped_refptr<URLLoaderAssets> url_loader_assets,
      blink::mojom::ServiceWorkerEventStatus status);

  ServiceWorkerMetrics::EventType GetEventType() const;

  bool IsEventDispatched() const;

  blink::mojom::FetchAPIRequestPtr request_;
  std::string client_id_;
  std::string resulting_client_id_;
  scoped_refptr<ServiceWorkerVersion> version_;
  const network::mojom::RequestDestination destination_;
  base::OnceClosure prepare_callback_;
  FetchCallback fetch_callback_;

  scoped_refptr<URLLoaderAssets> url_loader_assets_;

  // Holds the URLLoaderClient for the service worker to receive the navigation
  // preload response. It's passed to the service worker along with the fetch
  // event.
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      preload_url_loader_client_receiver_;

  base::UnguessableToken race_network_request_token_;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      race_network_request_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerFetchDispatcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_DISPATCHER_H_
