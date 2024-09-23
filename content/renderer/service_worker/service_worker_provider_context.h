// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_worker_client_registry.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_context.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace content {

namespace service_worker_provider_context_unittest {
class ServiceWorkerProviderContextTest;
FORWARD_DECLARE_TEST(ServiceWorkerProviderContextTest,
                     SetControllerServiceWorker);
FORWARD_DECLARE_TEST(ServiceWorkerProviderContextTest,
                     ControllerWithoutFetchHandler);
}  // namespace service_worker_provider_context_unittest

class WebServiceWorkerProviderImpl;
struct ServiceWorkerProviderContextDeleter;

// ServiceWorkerProviderContext stores common state for "providers" for service
// worker clients (currently WebServiceWorkerProviderImpl and
// ServiceWorkerNetworkProviderForFrame). Providers for the same underlying
// entity hold strong references to a shared instance of this class.
//
// ServiceWorkerProviderContext is also a
// blink::mojom::ServiceWorkerWorkerClientRegistry. If it's a provider for a
// document, then it tracks all the dedicated workers created from the document
// (including nested workers), as dedicated workers don't yet have their own
// providers. If it's a provider for a shared worker, then it tracks only the
// shared worker itself.
//
// Created and destructed on the main thread. Unless otherwise noted, all
// methods are called on the main thread.
class CONTENT_EXPORT ServiceWorkerProviderContext
    : public base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                        ServiceWorkerProviderContextDeleter>,
      public blink::WebServiceWorkerProviderContext,
      public blink::mojom::ServiceWorkerContainer,
      public blink::mojom::ServiceWorkerWorkerClientRegistry {
 public:
  // |receiver| is connected to the content::ServiceWorkerContainerHost that
  // notifies of changes to the registration's and workers' status.
  //
  // |controller_info| contains the endpoint and object info that is needed to
  // set up the controller service worker for the context.
  //
  // |fallback_loader_factory| is a default loader factory for fallback
  // requests, and is used when we create a subresource loader for controllees.
  // This is non-null only if the provider is created for controllees, and if
  // the loading context, e.g. a frame, provides it.
  ServiceWorkerProviderContext(
      blink::mojom::ServiceWorkerContainerType container_type,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainer>
          receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainerHost>
          host_remote,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  ServiceWorkerProviderContext(const ServiceWorkerProviderContext&) = delete;
  ServiceWorkerProviderContext& operator=(const ServiceWorkerProviderContext&) =
      delete;

  blink::mojom::ServiceWorkerContainerType container_type() const {
    return container_type_;
  }

  // Returns version id of the controller service worker object
  // (ServiceWorkerContainer#controller).
  int64_t GetControllerVersionId() const;

  // Takes the controller service worker object info set by SetController() if
  // any, otherwise returns nullptr.
  blink::mojom::ServiceWorkerObjectInfoPtr TakeController();

  // Returns the factory for loading subresources with the controller
  // ServiceWorker, or nullptr if no controller is attached. Returns a
  // WeakWrapperSharedURLLoaderFactory because the inner factory is destroyed
  // when this context is destroyed but loaders may persist a reference to the
  // loader returned from this method.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
  GetSubresourceLoaderFactory();

  // Returns the feature usage of the controller service worker.
  const std::set<blink::mojom::WebFeature>& used_features() const;

  // For providers for frames. See |fetch_request_window_id| in
  // network::ResourceRequest.
  const base::UnguessableToken& fetch_request_window_id() const;

  // Sets a weak pointer back to the WebServiceWorkerProviderImpl (which
  // corresponds to ServiceWorkerContainer in JavaScript) which has a strong
  // reference to |this|. This allows us to notify the
  // WebServiceWorkerProviderImpl when ServiceWorkerContainer#controller should
  // be changed.
  void SetWebServiceWorkerProvider(
      base::WeakPtr<WebServiceWorkerProviderImpl> provider);

  // blink::mojom::ServiceWorkerWorkerClientRegistry:
  void RegisterWorkerClient(
      mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClient>
          pending_client) override;
  void CloneWorkerClientRegistry(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClientRegistry>
          receiver) override;

  // Called when WebServiceWorkerNetworkProvider is destructed. This function
  // severs the Mojo binding to the browser-side ServiceWorkerContainerHost. The
  // reason WebServiceWorkerNetworkProvider is special compared to the other
  // providers, is that it is destructed synchronously when a service worker
  // client (Document) is removed from the DOM. Once this happens, the
  // ServiceWorkerContainerHost must destruct quickly in order to remove the
  // ServiceWorkerClient from the system (thus allowing unregistration/update to
  // occur and ensuring the Clients API doesn't return the client).
  //
  // TODO(crbug.com/41441021): Remove this weird partially destroyed
  // state.
  void OnNetworkProviderDestroyed();

  // May be nullptr if OnNetworkProviderDestroyed() has already been called.
  // Currently this can be called only for clients that are Documents,
  // see comments of |container_host_|.
  blink::mojom::ServiceWorkerContainerHost* container_host() const;

  // Called when blink::IdlenessDetector emits its network idle signal. Tells
  // the browser process that this page is quiet soon after page load, as a
  // hint to start the service worker update check.
  void DispatchNetworkQuiet();

  // Tells the container host that this context is execution ready:
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  void NotifyExecutionReady();

  // WebServiceWorkerProviderContext implementation.
  void BindServiceWorkerWorkerClientRemote(
      blink::CrossVariantMojoRemote<
          blink::mojom::ServiceWorkerWorkerClientInterfaceBase> pending_client)
      override;
  void BindServiceWorkerWorkerClientRegistryReceiver(
      blink::CrossVariantMojoReceiver<
          blink::mojom::ServiceWorkerWorkerClientRegistryInterfaceBase>
          receiver) override;
  blink::CrossVariantMojoRemote<
      blink::mojom::ServiceWorkerContainerHostInterfaceBase>
  CloneRemoteContainerHost() override;
  // SetController must be called before these functions.
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  blink::mojom::ServiceWorkerFetchHandlerType GetFetchHandlerType()
      const override;
  blink::mojom::ServiceWorkerFetchHandlerBypassOption
  GetFetchHandlerBypassOption() const override;
  const blink::WebString client_id() const override;

 private:
  friend class base::DeleteHelper<ServiceWorkerProviderContext>;
  friend class base::RefCountedThreadSafe<ServiceWorkerProviderContext,
                                          ServiceWorkerProviderContextDeleter>;
  friend class service_worker_provider_context_unittest::
      ServiceWorkerProviderContextTest;
  friend struct ServiceWorkerProviderContextDeleter;
  FRIEND_TEST_ALL_PREFIXES(service_worker_provider_context_unittest::
                               ServiceWorkerProviderContextTest,
                           SetControllerServiceWorker);
  FRIEND_TEST_ALL_PREFIXES(service_worker_provider_context_unittest::
                               ServiceWorkerProviderContextTest,
                           ControllerWithoutFetchHandler);

  ~ServiceWorkerProviderContext() override;

  void DestructOnMainThread() const;

  // Clears the information of the ServiceWorkerWorkerClient of dedicated (or
  // shared) worker, when the connection to the worker is disconnected.
  void UnregisterWorkerFetchContext(blink::mojom::ServiceWorkerWorkerClient*);

  // Implementation of blink::mojom::ServiceWorkerContainer.
  void SetController(
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      bool should_notify_controllerchange) override;
  void PostMessageToClient(blink::mojom::ServiceWorkerObjectInfoPtr source,
                           blink::TransferableMessage message) override;
  void CountFeature(blink::mojom::WebFeature feature) override;

  // A convenient utility method to tell if a subresource loader factory
  // can be created for this context.
  bool CanCreateSubresourceLoaderFactory() const;

  // Returns URLLoaderFactory for loading subresources with the controller
  // ServiceWorker, or nullptr.
  //
  // If the router evaluation is needed, this function always returns
  // URLLoaderFactory for subresources. the URLLoaderFactory can be created
  // without the controller ServiceWorker if |remote_controller_| is null, that
  // happens when there is no fetch handler. This behavior is needed because the
  // router evaluation is done in the ServiceWorkerSubresourceLoader.
  //
  // If the router evaluation is not needed, this function returns nullptr if no
  // controller is attached (e.g. no fetch handler), or the fetch handler
  // is no-op.
  network::mojom::URLLoaderFactory* GetSubresourceLoaderFactoryInternal();

  const blink::mojom::ServiceWorkerContainerType container_type_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // This keeps the connection to the content::ServiceWorkerContainerHost in the
  // browser process alive.
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainer> receiver_;

  // The |container_host_| remote represents the connection to the
  // browser-side ServiceWorkerContainerHost, whose lifetime is bound to
  // |container_host_| via the Mojo connection. This may be nullptr if the Mojo
  // connection was broken in OnNetworkProviderDestroyed().
  //
  // The |container_host_| remote also implements functions for
  // navigator.serviceWorker, but all the methods that correspond to
  // navigator.serviceWorker.* can be used only if |this| is a provider
  // for a Document, as navigator.serviceWorker is currently only implemented
  // for Document (https://crbug.com/371690).
  // Note: Currently this is always bound on main thread.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainerHost>
      container_host_;

  // |controller_| will be set by SetController() and taken by TakeController().
  blink::mojom::ServiceWorkerObjectInfoPtr controller_;
  // Keeps version id of the current controller service worker object.
  int64_t controller_version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  // Used to intercept requests from the controllee and dispatch them
  // as events to the controller ServiceWorker.
  mojo::Remote<network::mojom::URLLoaderFactory> subresource_loader_factory_;

  // Used when we create |subresource_loader_factory_|.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory_;

  // Used to ensure handed out loader factories are properly detached when the
  // contained subresource_loader_factory goes away.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      weak_wrapped_subresource_loader_factory_;

  // The Client#id value for this context.
  std::string client_id_;

  // Corresponds to a request's "window" in the Fetch spec:
  // https://fetch.spec.whatwg.org/#concept-request-window
  base::UnguessableToken fetch_request_window_id_;

  blink::mojom::ControllerServiceWorkerMode controller_mode_ =
      blink::mojom::ControllerServiceWorkerMode::kNoController;

  blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type_ =
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
  bool need_router_evaluate_ = false;

  blink::mojom::ServiceWorkerFetchHandlerBypassOption
      fetch_handler_bypass_option_ =
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::kDefault;

  std::optional<std::string> sha256_script_checksum_;

  std::optional<blink::ServiceWorkerRouterRules> router_rules_;
  // TODO(crbug.com/40941292): It may be better to make this an optional, so it
  // is possible to distinguish between unset and kStopped, which are not really
  // equivalent.
  blink::EmbeddedWorkerStatus initial_running_status_ =
      blink::EmbeddedWorkerStatus::kStopped;
  mojo::PendingRemote<blink::mojom::CacheStorage> remote_cache_storage_;
  mojo::PendingReceiver<blink::mojom::ServiceWorkerRunningStatusCallback>
      running_status_receiver_;

  // Tracks feature usage for UseCounter.
  std::set<blink::mojom::WebFeature> used_features_;

  // Corresponds to this context's ServiceWorkerContainer. May be null when not
  // yet created, when already destroyed, or when this client is not a Document
  // and therefore doesn't support navigator.serviceWorker.
  base::WeakPtr<WebServiceWorkerProviderImpl> web_service_worker_provider_;

  // Remotes for dedicated or shared workers which are associated with the
  // ServiceWorkerProviderContext.
  // - If this ServiceWorkerProviderContext is for a Document, then
  //   |worker_clients| contains all its dedicated workers.
  // - If this ServiceWorkerProviderContext is for a SharedWorker (technically
  //   speaking, for its shadow page), then |worker_clients| has one element:
  //   the shared worker.
  std::vector<mojo::Remote<blink::mojom::ServiceWorkerWorkerClient>>
      worker_clients_;

  // For adding new ServiceWorkerWorkerClients.
  mojo::ReceiverSet<blink::mojom::ServiceWorkerWorkerClientRegistry>
      worker_client_registry_receivers_;

  // Used in |subresource_loader_factory_| to get the connection to the
  // controller service worker.
  //
  // |remote_controller_| is a Mojo pipe to the controller service worker,
  // and is to be passed to (i.e. taken by) a subresource loader factory when
  // GetSubresourceLoaderFactory() is called for the first time when a valid
  // controller exists.
  //
  // |controller_connector_| is a Mojo pipe to the
  // ControllerServiceWorkerConnector that is attached to the newly created
  // subresource loader factory and lives on a background thread. This is
  // populated when GetSubresourceLoader() creates the subresource loader
  // factory and takes |controller_endpoint_|.
  mojo::PendingRemote<blink::mojom::ControllerServiceWorker> remote_controller_;
  mojo::Remote<blink::mojom::ControllerServiceWorkerConnector>
      controller_connector_;

  bool sent_execution_ready_ = false;
};

struct ServiceWorkerProviderContextDeleter {
  static void Destruct(const ServiceWorkerProviderContext* context) {
    context->DestructOnMainThread();
  }
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_CONTEXT_H_
