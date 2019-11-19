// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

class ServiceWorkerNavigationHandle;

struct ServiceWorkerNavigationLoaderInterceptorParams {
  // For all clients:
  ResourceType resource_type = ResourceType::kMainFrame;
  bool skip_service_worker = false;

  // For windows:
  bool is_main_frame = false;
  // Whether all ancestor frames of the frame that is navigating have a secure
  // origin. True for main frames.
  bool are_ancestors_secure = false;
  int frame_tree_node_id = RenderFrameHost::kNoFrameTreeNodeId;

  // For web workers:
  int process_id = ChildProcessHost::kInvalidUniqueID;
};

// Handles navigations for service worker clients (windows and web workers).
// Lives on the UI thread.
//
// The corresponding legacy class is ServiceWorkerControlleeRequestHandler which
// lives on the service worker context core thread. Currently, this class just
// delegates to the legacy class by posting tasks to it on the core thread.
class ServiceWorkerNavigationLoaderInterceptor final
    : public NavigationLoaderInterceptor {
 public:
  ServiceWorkerNavigationLoaderInterceptor(
      const ServiceWorkerNavigationLoaderInterceptorParams& params,
      base::WeakPtr<ServiceWorkerNavigationHandle> handle);
  ~ServiceWorkerNavigationLoaderInterceptor() override;

  // NavigationLoaderInterceptor overrides:

  // This could get called multiple times during the lifetime in redirect
  // cases. (In fallback-to-network cases we basically forward the request
  // to the request to the next request handler)
  void MaybeCreateLoader(const network::ResourceRequest& tentative_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override;
  // Returns params with the ControllerServiceWorkerInfoPtr if we have found
  // a matching controller service worker for the |request| that is given
  // to MaybeCreateLoader(). Otherwise this returns base::nullopt.
  base::Optional<SubresourceLoaderParams> MaybeCreateSubresourceLoaderParams()
      override;

  // These are called back from the core thread helper functions:
  void LoaderCallbackWrapper(
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      LoaderCallback loader_callback,
      SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread);
  void FallbackCallbackWrapper(FallbackCallback fallback_callback,
                               bool reset_subresource_loader_params);

  base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> GetWeakPtr();

 private:
  // Given as a callback to NavigationURLLoaderImpl.
  void RequestHandlerWrapper(
      SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread,
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // For navigations, |handle_| outlives |this|. It's owned by
  // NavigationRequest which outlives NavigationURLLoaderImpl which owns |this|.
  // For workers, |handle_| may be destroyed during interception. It's owned by
  // DedicatedWorkerHost or SharedWorkerHost which may be destroyed before
  // WorkerScriptLoader which owns |this|.
  // TODO(falken): Arrange things so |handle_| outlives |this| for workers too.
  const base::WeakPtr<ServiceWorkerNavigationHandle> handle_;

  const ServiceWorkerNavigationLoaderInterceptorParams params_;

  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  base::WeakPtrFactory<ServiceWorkerNavigationLoaderInterceptor> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationLoaderInterceptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_LOADER_INTERCEPTOR_H_
