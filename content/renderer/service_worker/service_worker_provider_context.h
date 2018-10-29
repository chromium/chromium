// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/renderer/service_worker/service_worker_provider_state_for_client.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

namespace service_worker_provider_context_unittest {
class ServiceWorkerProviderContextTest;
}  // namespace service_worker_provider_context_unittest

class WebServiceWorkerRegistrationImpl;
struct ServiceWorkerProviderContextDeleter;

// ServiceWorkerProviderContext stores common state for service worker
// "providers" (currently WebServiceWorkerProviderImpl,
// ServiceWorkerNetworkProvider, and ServiceWorkerContextClient). Providers for
// the same underlying entity hold strong references to a shared instance of
// this class.
//
// ServiceWorkerProviderContext is also a
// mojom::ServiceWorkerWorkerClientRegistry. If it's a provider for a document,
// then it tracks all the dedicated workers created from the document (including
// nested workers), as dedicated workers don't yet have their own providers. If
// it's a provider for a shared worker, then it tracks only the shared worker
// itself.
//
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread.
class CONTENT_EXPORT ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter>,
      public mojom::ServiceWorkerContainer,
      public mojom::ServiceWorkerWorkerClientRegistry {
 public:
  // Constructor for service worker clients.
  //
  // |provider_id| is used to identify this provider in IPC messages to the
  // browser process. |request| is an endpoint which is connected to
  // the content::ServiceWorkerProviderHost that notifies of changes to the
  // registration's and workers' status. |request| is bound with |binding_|.
  //
  // |controller_info| contains the endpoint (which is non-null only when
  // S13nServiceWorker is enabled) and object info that is needed to set up the
  // controller service worker for the client.
  // For S13nServiceWorker:
  // |fallback_loader_factory| is a default loader factory for fallback
  // requests, and is used when we create a subresource loader for controllees.
  // This is non-null only if the provider is created for controllees, and if
  // the loading context, e.g. a frame, provides it.
  ServiceWorkerProviderContext(
      int provider_id,
      blink::mojom::ServiceWorkerProviderType provider_type,
      mojom::ServiceWorkerContainerAssociatedRequest request,
      mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info,
      mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  // Constructor for service worker execution contexts.
  ServiceWorkerProviderContext(
      int provider_id,
      mojom::ServiceWorkerContainerAssociatedRequest request,
      mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info);

  blink::mojom::ServiceWorkerProviderType provider_type() const {
    return provider_type_;
  }

  int provider_id() const { return provider_id_; }

  // For service worker clients. Returns version id of the controller service
  // worker object (ServiceWorkerContainer#controller).
  int64_t GetControllerVersionId() const;

  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker() const;

  // For service worker clients. Takes the controller service worker object info
  // set by SetController() if any, otherwise returns nullptr.
  blink::mojom::ServiceWorkerObjectInfoPtr TakeController();

  // S13nServiceWorker:
  // For service worker clients. Returns URLLoaderFactory for loading
  // subresources with the controller ServiceWorker, or nullptr if
  // no controller is attached.
  network::mojom::URLLoaderFactory* GetSubresourceLoaderFactory();

  // For service worker clients. Returns the feature usage of its controller.
  const std::set<blink::mojom::WebFeature>& used_features() const;

  // S13nServiceWorker:
  // The Client#id value of the client.
  const std::string& client_id() const;

  // For service worker clients. Sets a weak pointer back to the
  // WebServiceWorkerProviderImpl (which corresponds to ServiceWorkerContainer
  // in JavaScript) which has a strong reference to |this|. This allows us to
  // notify the WebServiceWorkerProviderImpl when
  // ServiceWorkerContainer#controller should be changed.
  void SetWebServiceWorkerProvider(
      base::WeakPtr<WebServiceWorkerProviderImpl> provider);

  // mojom::ServiceWorkerWorkerClientRegistry:
  void RegisterWorkerClient(
      mojom::ServiceWorkerWorkerClientPtr client) override;
  void CloneWorkerClientRegistry(
      mojom::ServiceWorkerWorkerClientRegistryRequest request) override;

  // S13nServiceWorker:
  // For service worker clients. Returns a ServiceWorkerContainerHostPtrInfo
  // to this client's container host.
  mojom::ServiceWorkerContainerHostPtrInfo CloneContainerHostPtrInfo();

  // Called when ServiceWorkerNetworkProvider is destructed. This function
  // severs the Mojo binding to the browser-side ServiceWorkerProviderHost. The
  // reason ServiceWorkerNetworkProvider is special compared to the other
  // providers, is that it is destructed synchronously when a service worker
  // client (Document) is removed from the DOM. Once this happens, the
  // ServiceWorkerProviderHost must destruct quickly in order to remove the
  // ServiceWorkerClient from the system (thus allowing unregistration/update to
  // occur and ensuring the Clients API doesn't return the client).
  void OnNetworkProviderDestroyed();

  // Gets the mojom::ServiceWorkerContainerHost* for sending requests to
  // browser-side ServiceWorkerProviderHost. May be nullptr if
  // OnNetworkProviderDestroyed() has already been called.
  // Currently this can be called only for clients that are Documents,
  // see comments of |container_host_|.
  mojom::ServiceWorkerContainerHost* container_host() const;

  // Pings the container host and calls |callback| once a pong arrived. Useful
  // for waiting for all messages the host sent thus far to arrive.
  void PingContainerHost(base::OnceClosure callback);

  // Called when blink::IdlenessDetector emits its network idle signal. Tells
  // the browser process that this page is quiet soon after page load, as a
  // hint to start the service worker update check.
  void DispatchNetworkQuiet();

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend class service_worker_provider_context_unittest::
      ServiceWorkerProviderContextTest;
  friend class WebServiceWorkerRegistrationImpl;
  friend struct ServiceWorkerProviderContextDeleter;

  ~ServiceWorkerProviderContext() override;
  void DestructOnMainThread() const;

  // Clears the information of the ServiceWorkerWorkerClient of dedicated (or
  // shared) worker, when the connection to the worker is disconnected.
  void UnregisterWorkerFetchContext(mojom::ServiceWorkerWorkerClient*);

  // Implementation of mojom::ServiceWorkerContainer.
  void SetController(mojom::ControllerServiceWorkerInfoPtr controller_info,
                     const std::vector<blink::mojom::WebFeature>& used_features,
                     bool should_notify_controllerchange) override;
  void PostMessageToClient(blink::mojom::ServiceWorkerObjectInfoPtr source,
                           blink::TransferableMessage message) override;
  void CountFeature(blink::mojom::WebFeature feature) override;

  // S13nServiceWorker:
  // For service worker clients.
  // A convenient utility method to tell if a subresource loader factory
  // can be created for this client.
  bool CanCreateSubresourceLoaderFactory() const;

  const blink::mojom::ServiceWorkerProviderType provider_type_;
  const int provider_id_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Mojo binding for the |request| passed to the constructor. This keeps the
  // connection to the content::ServiceWorkerProviderHost in the browser process
  // alive.
  mojo::AssociatedBinding<mojom::ServiceWorkerContainer> binding_;

  // The |container_host_| interface represents the connection to the
  // browser-side ServiceWorkerProviderHost, whose lifetime is bound to
  // |container_host_| via the Mojo connection.
  // The |container_host_| interface also implements functions for
  // navigator.serviceWorker, but all the methods that correspond to
  // navigator.serviceWorker.* can be used only if |this| is a provider
  // for a Document, as navigator.serviceWorker is currently only implemented
  // for Document (https://crbug.com/371690).
  // Note: Currently this is always bound on main thread.
  mojom::ServiceWorkerContainerHostAssociatedPtr container_host_;

  // State for service worker clients.
  std::unique_ptr<ServiceWorkerProviderStateForClient> state_for_client_;

  // NOTE: Add new members to |state_for_client_| if they are relevant only for
  // service worker clients. Not here!

  base::WeakPtrFactory<ServiceWorkerProviderContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderContext);
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
