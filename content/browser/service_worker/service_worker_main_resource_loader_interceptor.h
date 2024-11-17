// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

class ServiceWorkerMainResourceHandle;
struct NavigationRequestInfo;

// Handles navigations for service worker clients (windows and web workers).
// Lives on the UI thread.
//
// The corresponding legacy class is ServiceWorkerControlleeRequestHandler which
// used to live on a different thread. Currently, this class just delegates to
// the legacy class.
// TODO(crbug.com/40725202): Merge the classes together now that they are on
// the same thread.
class CONTENT_EXPORT ServiceWorkerMainResourceLoaderInterceptor final
    : public NavigationLoaderInterceptor {
 public:
  // Creates a ServiceWorkerMainResourceLoaderInterceptor for a navigation.
  // Returns nullptr if the interceptor could not be created for this |url|.
  static std::unique_ptr<NavigationLoaderInterceptor> CreateForNavigation(
      const GURL& url,
      base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle,
      const NavigationRequestInfo& request_info);

  // Creates a ServiceWorkerMainResourceLoaderInterceptor for a worker.
  // Returns nullptr if the interceptor could not be created for the URL of the
  // worker.
  static std::unique_ptr<ServiceWorkerMainResourceLoaderInterceptor>
  CreateForWorker(
      const network::ResourceRequest& resource_request,
      const net::IsolationInfo& isolation_info,
      int process_id,
      const DedicatedOrSharedWorkerToken& worker_token,
      base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle);

  ServiceWorkerMainResourceLoaderInterceptor(
      const ServiceWorkerMainResourceLoaderInterceptor&) = delete;
  ServiceWorkerMainResourceLoaderInterceptor& operator=(
      const ServiceWorkerMainResourceLoaderInterceptor&) = delete;

  ~ServiceWorkerMainResourceLoaderInterceptor() override;

  // NavigationLoaderInterceptor overrides:

  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;

  // MaybeCreateLoaderForResponse() should NOT overridden here, because
  // `WorkerScriptLoader` assumes so.

  static void CompleteWithoutLoader(
      NavigationLoaderInterceptor::LoaderCallback loader_callback,
      base::WeakPtr<ServiceWorkerClient> service_worker_client);

 private:
  friend class ServiceWorkerMainResourceLoaderInterceptorTest;

  ServiceWorkerMainResourceLoaderInterceptor(
      base::WeakPtr<ServiceWorkerMainResourceHandle> handle,
      bool skip_service_worker,
      FrameTreeNodeId frame_tree_node_id,
      const net::IsolationInfo& isolation_info);

  // Returns true if a ServiceWorkerMainResourceLoaderInterceptor should be
  // created for a navigation to |url|.
  static bool ShouldCreateForNavigation(
      const GURL& url,
      network::mojom::RequestDestination request_destination,
      BrowserContext* browser_context);

  // Given as a callback to NavigationURLLoaderImpl.
  void RequestHandlerWrapper(
      network::SingleRequestURLLoaderFactory::RequestHandler handler,
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // For navigations, |handle_| outlives |this|. It's owned by
  // NavigationRequest which outlives NavigationURLLoaderImpl which owns |this|.
  // For workers, |handle_| may be destroyed during interception. It's owned by
  // DedicatedWorkerHost or SharedWorkerHost which may be destroyed before
  // WorkerScriptLoader which owns |this|.
  // TODO(falken): Arrange things so |handle_| outlives |this| for workers too.
  const base::WeakPtr<ServiceWorkerMainResourceHandle> handle_;

  // For all clients:
  const bool skip_service_worker_;

  // Updated on redirects.
  net::IsolationInfo isolation_info_;

  // For window clients:
  // If the intercepted resource load is on behalf
  // of a window, the |frame_tree_node_id_| will be set.
  const FrameTreeNodeId frame_tree_node_id_;

  // Handles a single request. Set to a new instance on redirects.
  std::unique_ptr<ServiceWorkerControlleeRequestHandler> request_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_LOADER_INTERCEPTOR_H_
