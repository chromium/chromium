// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include <map>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/loader/keep_alive_attribution_request_helper.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

namespace content {

KeepAliveURLLoaderService::FactoryContext::FactoryContext(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    scoped_refptr<PolicyContainerHost> frame_policy_container_host)
    : factory(shared_url_loader_factory),
      policy_container_host(std::move(frame_policy_container_host)) {
  CHECK(policy_container_host);
}

KeepAliveURLLoaderService::FactoryContext::FactoryContext(
    const std::unique_ptr<FactoryContext>& other)
    : factory(other->factory),
      weak_document_ptr(other->weak_document_ptr),
      policy_container_host(other->policy_container_host),
      attribution_context(other->attribution_context) {}

KeepAliveURLLoaderService::FactoryContext::~FactoryContext() = default;

void KeepAliveURLLoaderService::FactoryContext::OnDidCommitNavigation(
    WeakDocumentPtr committed_document) {
  weak_document_ptr = committed_document;

  CHECK(weak_document_ptr.AsRenderFrameHostIfValid());
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      weak_document_ptr.AsRenderFrameHostIfValid());
  policy_container_host = rfh->policy_container_host();
  attribution_context = AttributionSuitableContext::Create(rfh->GetGlobalId());

  CHECK(policy_container_host);
}

void KeepAliveURLLoaderService::FactoryContext::UpdateFactory(
    scoped_refptr<network::SharedURLLoaderFactory> new_factory) {
  factory = new_factory;
}

// KeepAliveURLLoaderFactoriesBase is an abstract base class for creating and
// managing all the KeepAliveURLLoader instances created by multiple factories
// of the same `Interface`.
//
// The receivers of those factories should be managed in subclass, which
// implements `Interface` to bind factories from different RFHs.
//
// `Interface` supports mojo interfaces other than URLLoaderFactory as long as
// the provided arguments to a call to Interface::CreateLoader() able to satisfy
// KeepAliveURLLoader ctor's requirement.
//
// The lifetime of an instance of a subclass must be the same as the owning
// KeepAliveURLLoaderService.
template <typename Interface,
          template <typename>
          class PendingReceiverType,
          template <typename, typename>
          class ReceiverSetType>
class KeepAliveURLLoaderService::KeepAliveURLLoaderFactoriesBase {
 public:
  explicit KeepAliveURLLoaderFactoriesBase(KeepAliveURLLoaderService* service)
      : service_(service) {
    // `Unretained(this)` is safe because `this` owns `loader_receivers_`.
    loader_receivers_.set_disconnect_handler(base::BindRepeating(
        &KeepAliveURLLoaderFactoriesBase::OnLoaderDisconnected,
        base::Unretained(this)));
  }
  // Not copyable.
  KeepAliveURLLoaderFactoriesBase(const KeepAliveURLLoaderFactoriesBase&) =
      delete;
  KeepAliveURLLoaderFactoriesBase& operator=(
      const KeepAliveURLLoaderFactoriesBase&) = delete;

  // Called when the BrowserContext that owns `service_` is shutting down.
  void Shutdown() {
    // Notifies every loader synchronously which may not have chance to start
    // loading before shutting down.
    for (const auto& [_, weak_ptr_loader] : weak_ptr_loaders_) {
      if (weak_ptr_loader) {
        weak_ptr_loader->Shutdown();
      }
    }
  }

  // For testing only:
  size_t NumLoadersForTesting() const {
    return loader_receivers_.size() + disconnected_loaders_.size();
  }
  size_t NumDisconnectedLoadersForTesting() const {
    return disconnected_loaders_.size();
  }

 protected:
  // Creates a new KeepAliveURLLoader from the factory of the `context` to load
  // `resource_request`. It returns a raw pointer to the loader already stored
  // in `service()`. Note that the caller must manually start the returned
  // loader.
  //
  // On calling, the initiator renderer that triggers this factory method
  // may have already be gone, e.g. a keepalive request initiated from an
  // unload handler. But as long as `context` exists, the necessary data for a
  // loader is ensured to exist.
  raw_ptr<KeepAliveURLLoader> CreateKeepAliveURLLoader(
      PendingReceiverType<Interface> receiver,
      const std::unique_ptr<FactoryContext>& context,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    CHECK(context);
    if (!resource_request.keepalive) {
      mojo::ReportBadMessage(
          "Unexpected `resource_request` in "
          "KeepAliveURLLoaderFactoriesBase::CreateLoaderAndStart(): "
          "resource_request.keepalive must be true");
      return nullptr;
    }
    if (resource_request.trusted_params) {
      // Must use untrusted URLLoaderFactory. If not, the requesting renderer
      // should be aborted.
      mojo::ReportBadMessage(
          "Unexpected `resource_request` in "
          "KeepAliveURLLoaderFactoriesBase::CreateLoaderAndStart(): "
          "resource_request.trusted_params must not be set");
      return nullptr;
    }

    // Passes in the pending remote of `client` from a renderer so that `loader`
    // can forward response back to the renderer.
    CHECK(context->policy_container_host);
    auto loader = std::make_unique<KeepAliveURLLoader>(
        request_id, options, resource_request, std::move(client),
        traffic_annotation, context->factory,
        // `context` can be destroyed right at the end of this method if the
        // caller renderer is already unloaded, meaning `loader` also needs to
        // hold another refptr to ensure `PolicyContainerHost` alive.
        context->policy_container_host, context->weak_document_ptr,
        service_->browser_context_,
        base::BindRepeating(&KeepAliveURLLoaderFactoriesBase::CreateThrottles,
                            base::Unretained(this), resource_request),
        base::PassKey<KeepAliveURLLoaderService>(),
        context->attribution_context.has_value()
            ? KeepAliveAttributionRequestHelper::CreateIfNeeded(
                  resource_request.attribution_reporting_eligibility,
                  resource_request.url,
                  resource_request.attribution_reporting_src_token,
                  resource_request.devtools_request_id,
                  context->attribution_context.value())
            : nullptr);
    // Adds a new loader receiver to the set held by `this`, binding the pending
    // `receiver` from a renderer to `raw_loader` with `loader` as its context.
    // The set will keep `loader` alive.
    auto* raw_loader = loader.get();
    auto receiver_id = loader_receivers_.Add(raw_loader, std::move(receiver),
                                             std::move(loader));
    raw_loader->set_on_delete_callback(
        base::BindOnce(&KeepAliveURLLoaderFactoriesBase::RemoveLoader,
                       base::Unretained(this), receiver_id));
    weak_ptr_loaders_.emplace(receiver_id, std::move(raw_loader->GetWeakPtr()));

    if (service_->loader_test_observer_) {
      raw_loader->SetObserverForTesting(     // IN-TEST
          service_->loader_test_observer_);  // IN-TEST
    }

    return raw_loader;
  }

 private:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      const network::ResourceRequest& resource_request) {
    if (service_->url_loader_throttles_getter_for_testing_) {
      return service_->url_loader_throttles_getter_for_testing_
          .Run();  // IN-TEST
    }

    // These throttles are also run by `blink::ThrottlingURLLoader`. However,
    // they have to be re-run here in case of handling in-browser redirects.
    // There is already a similar use case that also runs throttles in browser
    // in `SearchPrefetchRequest::StartPrefetchRequest()`. The review discussion
    // in https://crrev.com/c/2552723/3 suggests that running them again in
    // browser is fine.
    return CreateContentBrowserURLLoaderThrottlesForKeepAlive(
        resource_request, service_->browser_context_,
        // The renderer might be gone at any point when a throttle is running in
        // the KeepAliveURLLoader.
        /*wc_getter=*/base::BindRepeating([]() -> WebContents* {
          return nullptr;
        }),
        FrameTreeNodeId());
  }

  void OnLoaderDisconnected() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto disconnected_loader_receiver_id = loader_receivers_.current_receiver();

    // The context of `disconnected_loader_receiver_id`, an KeepAliveURLLoader
    // object, has been removed from `loader_receivers_`, but it has to stay
    // alive to handle subsequent updates from network service.

    // Let the KeepAliveURLLoader object itself aware of being disconnected.
    loader_receivers_.current_context()->OnURLLoaderDisconnected();

    // Move all KeepAliveURLLoader objects into a different loader set to keep
    // them alive until finish or being dropped.
    disconnected_loaders_.emplace(
        disconnected_loader_receiver_id,
        std::move(loader_receivers_.current_context()));
  }

  void RemoveLoader(mojo::ReceiverId loader_receiver_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading", "KeepAliveURLLoaderFactoriesBase::RemoveLoader",
                "loader_id", loader_receiver_id);

    loader_receivers_.Remove(loader_receiver_id);
    disconnected_loaders_.erase(loader_receiver_id);
    weak_ptr_loaders_.erase(loader_receiver_id);
  }

  // Guaranteed to exist, as `service_` owns this.
  raw_ptr<KeepAliveURLLoaderService> service_;

  // Holds all the KeepAliveURLLoader connected with remotes in renderers.
  // Each of them corresponds to the handling of one pending keepalive request.
  // Once a receiver is disconnected, its context should be moved to
  // `disconnected_loaders_`.
  ReceiverSetType<Interface, std::unique_ptr<KeepAliveURLLoader>>
      loader_receivers_;

  // Holds all the KeepAliveURLLoader that has been disconnected from renderers.
  // They should be kept alive until the request completes or fails.
  // The key is the mojo::ReceiverId assigned by `loader_receivers_`.
  std::map<mojo::ReceiverId, std::unique_ptr<KeepAliveURLLoader>>
      disconnected_loaders_;

  // Stores WeakPtr to all the instances of KeepAliveURLLoader created by this
  // group of factories, i.e. loaders from `loader_receivers_` and
  // `disconnected_loaders_`, to allow direct access to them when necessary.
  std::map<mojo::ReceiverId, base::WeakPtr<KeepAliveURLLoader>>
      weak_ptr_loaders_;
};

// A mojom::URLLoaderFactory to handle fetch keepalive requests.
//
// This factory can handle requests from multiple remotes of URLLoaderFactory
// from different renderers.
//
// Users should call `BindFactory()` first to register a pending receiver with
// this factory. A receiver stays until it gets disconnected from its remote in
// a renderer. Hence, its lifetime is roughly equal to the lifetime of its
// initiating renderer.
//
// On requested by a remote, i.e. calling
// `network::mojom::URLLoaderFactory::CreateLoaderAndStart()`, this factory will
// create a `KeepAliveURLLoader` to load a keepalive request. The loader will be
// held by the parent class until it either completes or fails to load the
// request. Note that a loader may outlive the FactoryContext that has created
// itself.
//
// See the "Implementation Details" section of the design doc
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class KeepAliveURLLoaderService::KeepAliveURLLoaderFactories final
    : public KeepAliveURLLoaderService::KeepAliveURLLoaderFactoriesBase<
          network::mojom::URLLoader,
          mojo::PendingReceiver,
          mojo::ReceiverSet>,
      public network::mojom::URLLoaderFactory {
 public:
  explicit KeepAliveURLLoaderFactories(KeepAliveURLLoaderService* service)
      : KeepAliveURLLoaderFactoriesBase(service) {}

  // Creates a `FactoryContext` to hold a refptr to
  // `network::SharedURLLoaderFactory`, which is constructed with
  // `subresource_proxying_factory_bundle`, and then bound with `receiver`.
  // `policy_container_host` must not be null.
  base::WeakPtr<FactoryContext> BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      scoped_refptr<PolicyContainerHost> policy_container_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(policy_container_host);
    TRACE_EVENT("loading", "KeepAliveURLLoaderFactories::BindFactory");

    // Adds a new factory receiver to the set, binding the pending `receiver`
    // from to `this` with a new context that has frame-specific data and keeps
    // reference to `subresource_proxying_factory_bundle`.
    auto context = std::make_unique<FactoryContext>(
        std::move(subresource_proxying_factory_bundle),
        std::move(policy_container_host));
    auto weak_context = context->weak_ptr_factory.GetWeakPtr();
    loader_factory_receivers_.Add(this, std::move(receiver),
                                  std::move(context));
    return weak_context;
  }

  // `network::mojom::URLLoaderFactory` overrides:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading", "KeepAliveURLLoaderFactories::CreateLoaderAndStart",
                "request_id", request_id);
    if (!base::FeatureList::IsEnabled(
            blink::features::kKeepAliveInBrowserMigration)) {
      mojo::ReportBadMessage(
          "Unexpected call to "
          "KeepAliveURLLoaderFactories::CreateLoaderAndStart()");
      return;
    }
    if (resource_request.is_fetch_later_api) {
      mojo::ReportBadMessage(
          "Unexpected `resource_request.is_fetch_later_api` in "
          "KeepAliveURLLoaderFactories::CreateLoaderAndStart(): "
          "must not be set");
      return;
    }

    auto raw_loader = CreateKeepAliveURLLoader(
        std::move(receiver), loader_factory_receivers_.current_context(),
        request_id, options, resource_request, std::move(client),
        traffic_annotation);
    if (!raw_loader) {
      return;
    }

    // `raw_loader` must only be started after the above setup.
    // For non-FetchLater requests, they should be started immediately.
    raw_loader->Start();
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    loader_factory_receivers_.Add(
        this, std::move(receiver),
        std::make_unique<FactoryContext>(
            loader_factory_receivers_.current_context()));
  }

 private:
  // Guaranteed to exist, as `service_` owns this object.
  raw_ptr<KeepAliveURLLoaderService> service_;

  // Receives `network::mojom::URLLoaderFactory` requests from renderers.
  // A receiver is added into this set after calling `BindFactory()`, and will
  // be removed once it is disconnected from the corresponding remote (usually
  // in a renderer).
  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    std::unique_ptr<FactoryContext>>
      loader_factory_receivers_;
};

// FetchLaterLoaderFactories handles requests from multiple remotes of
// FetchLaterLoaderFactory from different renderers.
//
// See also
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class KeepAliveURLLoaderService::FetchLaterLoaderFactories final
    : public KeepAliveURLLoaderService::KeepAliveURLLoaderFactoriesBase<
          blink::mojom::FetchLaterLoader,
          mojo::PendingAssociatedReceiver,
          mojo::AssociatedReceiverSet>,
      public blink::mojom::FetchLaterLoaderFactory {
 public:
  explicit FetchLaterLoaderFactories(KeepAliveURLLoaderService* service)
      : KeepAliveURLLoaderFactoriesBase(service) {}

  // Creates a `FactoryContext` to hold a refptr to `shared_url_loader_factory`,
  // and then bound with `receiver`.
  // `policy_container_host` must not be null.
  base::WeakPtr<FactoryContext> BindFactory(
      mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>
          receiver,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      scoped_refptr<PolicyContainerHost> policy_container_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(policy_container_host);
    TRACE_EVENT("loading", "FetchLaterLoaderFactories::BindFactory");

    // Adds a new factory receiver to the set, binding the pending `receiver`
    // from to `this` with a new context that has frame-specific data and keeps
    // reference to `shared_url_loader_factory`.
    auto context = std::make_unique<FactoryContext>(
        std::move(shared_url_loader_factory), std::move(policy_container_host));
    auto weak_context = context->weak_ptr_factory.GetWeakPtr();
    loader_factory_receivers_.Add(this, std::move(receiver),
                                  std::move(context));
    return weak_context;
  }

  // `blink::mojom::FetchLaterLoaderFactory` overrides:
  void CreateLoader(
      mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading", "FetchLaterLoaderFactories::CreateLoader",
                "request_id", request_id);
    if (!base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI)) {
      mojo::ReportBadMessage(
          "Unexpected call to "
          "FetchLaterLoaderFactories::CreateLoader()");
      return;
    }
    if (!resource_request.is_fetch_later_api) {
      mojo::ReportBadMessage(
          "Unexpected `resource_request.is_fetch_later_api` in "
          "FetchLaterLoaderFactories::CreateLoader(): must be set");
      return;
    }

    CreateKeepAliveURLLoader(std::move(receiver),
                             loader_factory_receivers_.current_context(),
                             request_id, options, resource_request,
                             mojo::NullRemote(), traffic_annotation);
    // See also `OnLoaderDisconnected()` for when `raw_loader` will be started.
  }

  void Clone(
      mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>
          receiver) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    loader_factory_receivers_.Add(
        this, std::move(receiver),
        std::make_unique<FactoryContext>(
            loader_factory_receivers_.current_context()));
  }

 private:
  // Receives `blink::mojom::FetchLaterLoaderFactory` requests from renderers.
  // A receiver is added into this set after calling `BindFactory()`, and will
  // be removed once it is disconnected from the corresponding remote in a
  // renderer.
  mojo::AssociatedReceiverSet<blink::mojom::FetchLaterLoaderFactory,
                              std::unique_ptr<FactoryContext>>
      loader_factory_receivers_;
};

KeepAliveURLLoaderService::KeepAliveURLLoaderService(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(browser_context_);

  url_loader_factories_ = std::make_unique<KeepAliveURLLoaderFactories>(this);
  fetch_later_loader_factories_ =
      std::make_unique<FetchLaterLoaderFactories>(this);
}

KeepAliveURLLoaderService::~KeepAliveURLLoaderService() = default;

base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
KeepAliveURLLoaderService::BindFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<network::SharedURLLoaderFactory>
        subresource_proxying_factory_bundle,
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(subresource_proxying_factory_bundle);
  CHECK(policy_container_host);

  return url_loader_factories_->BindFactory(
      std::move(receiver), std::move(subresource_proxying_factory_bundle),
      std::move(policy_container_host));
}

base::WeakPtr<KeepAliveURLLoaderService::FactoryContext>
KeepAliveURLLoaderService::BindFetchLaterLoaderFactory(
    mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>
        receiver,
    scoped_refptr<network::SharedURLLoaderFactory>
        subresource_proxying_factory_bundle,
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(subresource_proxying_factory_bundle);
  CHECK(policy_container_host);

  return fetch_later_loader_factories_->BindFactory(
      std::move(receiver), std::move(subresource_proxying_factory_bundle),
      std::move(policy_container_host));
}

void KeepAliveURLLoaderService::Shutdown() {
  // Only fetch_later_loader_factories_ needs shutdown notification to handle
  // its non-started loaders.
  fetch_later_loader_factories_->Shutdown();
  // Notifies fetch keepalive loader factories for it to log debugging metrics.
  url_loader_factories_->Shutdown();
}

size_t KeepAliveURLLoaderService::NumLoadersForTesting() const {
  return url_loader_factories_->NumLoadersForTesting() +         // IN-TEST
         fetch_later_loader_factories_->NumLoadersForTesting();  // IN-TEST
}

size_t KeepAliveURLLoaderService::NumDisconnectedLoadersForTesting() const {
  return url_loader_factories_->NumDisconnectedLoadersForTesting() +  // IN-TEST
         fetch_later_loader_factories_
             ->NumDisconnectedLoadersForTesting();  // IN-TEST
}

void KeepAliveURLLoaderService::SetLoaderObserverForTesting(
    scoped_refptr<KeepAliveURLLoader::TestObserver> observer) {
  loader_test_observer_ = observer;
}

void KeepAliveURLLoaderService::SetURLLoaderThrottlesGetterForTesting(
    KeepAliveURLLoader::URLLoaderThrottlesGetter
        url_loader_throttles_getter_for_testing) {
  url_loader_throttles_getter_for_testing_ =
      std::move(url_loader_throttles_getter_for_testing);
}

}  // namespace content
