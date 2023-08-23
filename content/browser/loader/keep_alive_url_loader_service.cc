// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

// A Context for the receiver of a KeepAliveURLLoaderFactory connection between
// a renderer and the browser.
//
// See `mojo::ReceiverSetBase` for more details.
struct FactoryContext {
  FactoryContext(scoped_refptr<network::SharedURLLoaderFactory> factory,
                 scoped_refptr<PolicyContainerHost> frame_policy_container_host)
      : factory(factory),
        policy_container_host(std::move(frame_policy_container_host)) {
    CHECK(policy_container_host);
  }

  explicit FactoryContext(const std::unique_ptr<FactoryContext>& other)
      : FactoryContext(other->factory, other->policy_container_host) {}

  ~FactoryContext() = default;
  // Not Copyable.
  FactoryContext(const FactoryContext&) = delete;
  FactoryContext& operator=(const FactoryContext&) = delete;

  // A refptr to the factory to use for the requests initiated from this
  // context.
  scoped_refptr<network::SharedURLLoaderFactory> factory;

  // A refptr to `PolicyContainerHost` of the RenderFrameHostImpl connecting to
  // the `KeepAliveURLLoaderFactory` using this context.
  //
  // This field keeps the pointed object alive such that any pending keepalive
  // redirect requests can still be verified against these same policies.
  scoped_refptr<PolicyContainerHost> policy_container_host;

  // This must be the last member.
  base::WeakPtrFactory<FactoryContext> weak_ptr_factory{this};
};

}  // namespace

// A mojom::URLLoaderFactory to handle fetch keepalive requests.
//
// This factory can handle requests from multiple remotes of URLLoaderFactory
// from different renderers.
// Users should call `BindFactory()` first to register a pending receiver with
// this factory.
//
// On requested by a remote, i.e. calling
// `network::mojom::URLLoaderFactory::CreateLoaderAndStart()`, this factory will
// create a `KeepAliveURLLoader` to load a keepalive request. The loader will be
// held by the `KeepAliveURLLoaderService` owning this factory until it either
// completes or fails to load the request. Note that a loader may outlive
// the FactoryContext that has created itself.
//
// This factory must be run in the browser process.
//
// See the "Implementation Details" section of the design doc
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class KeepAliveURLLoaderService::KeepAliveURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit KeepAliveURLLoaderFactory(KeepAliveURLLoaderService* service)
      : service_(service) {
    CHECK(service_);
  }
  ~KeepAliveURLLoaderFactory() override = default;

  // Not copyable.
  KeepAliveURLLoaderFactory(const KeepAliveURLLoaderFactory&) = delete;
  KeepAliveURLLoaderFactory& operator=(const KeepAliveURLLoaderFactory&) =
      delete;

  // Returns the pointer to the context for this factory.
  const std::unique_ptr<FactoryContext>& current_context() const {
    return loader_factory_receivers_.current_context();
  }

  // Creates a `FactoryContext` to hold a refptr to
  // `network::SharedURLLoaderFactory`, which is constructed with
  // `subresource_proxying_factory_bundle`, and then bound with `receiver`.
  // `policy_container_host` must not be null.
  void BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      scoped_refptr<PolicyContainerHost> policy_container_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(policy_container_host);
    TRACE_EVENT("loading", "KeepAliveURLLoaderFactory::BindFactory");

    // Adds a new factory receiver to the set, binding the pending `receiver`
    // from to `this` with a new context that has frame-specific data and keeps
    // reference to `subresource_proxying_factory_bundle`.
    auto context = std::make_unique<FactoryContext>(
        subresource_proxying_factory_bundle, std::move(policy_container_host));
    loader_factory_receivers_.Add(this, std::move(receiver),
                                  std::move(context));
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
    TRACE_EVENT("loading", "KeepAliveURLLoaderFactory::CreateLoaderAndStart",
                "request_id", request_id);

    if (!resource_request.keepalive) {
      loader_factory_receivers_.ReportBadMessage(
          "Unexpected `resource_request` in "
          "KeepAliveURLLoaderService::CreateLoaderAndStart(): "
          "resource_request.keepalive must be true");
      return;
    }
    if (resource_request.trusted_params) {
      // Must use untrusted URLLoaderFactory. If not, the requesting renderer
      // should be aborted.
      loader_factory_receivers_.ReportBadMessage(
          "Unexpected `resource_request` in "
          "KeepAliveURLLoaderService::CreateLoaderAndStart(): "
          "resource_request.trusted_params must not be set");
      return;
    }

    // Creates a new KeepAliveURLLoader from the factory of the current context
    // to load `resource_request`.
    //
    // At this point, the initiator renderer that triggers this factory method
    // may have already be gone, e.g. a keepalive request initiated from an
    // unload handler. But as long as `context` exists, the necessary data for a
    // loader is ensured to exist.
    const std::unique_ptr<FactoryContext>& context = current_context();

    // Passes in the pending remote of `client` from a renderer so that `loader`
    // can forward response back to the renderer.
    CHECK(context->policy_container_host);
    auto loader = std::make_unique<KeepAliveURLLoader>(
        request_id, options, resource_request, std::move(client),
        traffic_annotation, context->factory,
        // `context` can be destroyed right at the end of this method if the
        // caller renderer is already unloaded, meaning `loader` also needs to
        // hold another refptr to ensure `PolicyContainerHost` alive.
        context->policy_container_host, service_->browser_context_,
        CreateThrottles(resource_request),
        base::PassKey<KeepAliveURLLoaderService>());
    // Adds a new loader receiver to the set, binding the pending `receiver`
    // from a renderer to `raw_loader` with `loader` as its context. The set
    // will keep `loader` alive.
    auto* raw_loader = loader.get();
    auto receiver_id = service_->loader_receivers_.Add(
        raw_loader, std::move(receiver), std::move(loader));
    raw_loader->set_on_delete_callback(
        base::BindOnce(&KeepAliveURLLoaderService::RemoveLoader,
                       base::Unretained(service_), receiver_id));

    if (service_->loader_test_observer_) {
      raw_loader->SetObserverForTesting(     // IN-TEST
          service_->loader_test_observer_);  // IN-TEST
    }

    // `loader` must only be started after the above setup.
    if (!resource_request.is_fetch_later_api) {
      raw_loader->Start();
    }
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    loader_factory_receivers_.Add(
        this, std::move(receiver),
        std::make_unique<FactoryContext>(current_context()));
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
        FrameTreeNode::kFrameTreeNodeInvalidId);
  }

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

KeepAliveURLLoaderService::KeepAliveURLLoaderService(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(browser_context_);

  factory_ =
      std::make_unique<KeepAliveURLLoaderService::KeepAliveURLLoaderFactory>(
          this);
  // `Unretained(this)` is safe because `this` owns `loader_receivers_`.
  loader_receivers_.set_disconnect_handler(
      base::BindRepeating(&KeepAliveURLLoaderService::OnLoaderDisconnected,
                          base::Unretained(this)));
}

KeepAliveURLLoaderService::~KeepAliveURLLoaderService() = default;

void KeepAliveURLLoaderService::BindFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<network::SharedURLLoaderFactory>
        subresource_proxying_factory_bundle,
    scoped_refptr<PolicyContainerHost> policy_container_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(policy_container_host);

  factory_->BindFactory(std::move(receiver),
                        subresource_proxying_factory_bundle,
                        std::move(policy_container_host));
}

void KeepAliveURLLoaderService::OnLoaderDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto disconnected_loader_receiver_id = loader_receivers_.current_receiver();
  TRACE_EVENT("loading", "KeepAliveURLLoaderService::OnLoaderDisconnected",
              "loader_id", disconnected_loader_receiver_id);

  // The context of `disconnected_loader_receiver_id`, an KeepAliveURLLoader
  // object, has been removed from `loader_receivers_`, but it has to stay alive
  // to handle subsequent updates from network service.

  // First, check if the KeepAliveURLLoader object is pending to start.
  if (!loader_receivers_.current_context()->IsStarted()) {
    // Last chance to start a deferred loader here.
    loader_receivers_.current_context()->Start();
  }

  // Last, move the KeepAliveURLLoader object into a different loader set to
  // keep it alive until finish.
  disconnected_loaders_.emplace(disconnected_loader_receiver_id,
                                std::move(loader_receivers_.current_context()));
}

void KeepAliveURLLoaderService::RemoveLoader(
    mojo::ReceiverId loader_receiver_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoaderService::RemoveLoader", "loader_id",
              loader_receiver_id);

  loader_receivers_.Remove(loader_receiver_id);
  disconnected_loaders_.erase(loader_receiver_id);
}

size_t KeepAliveURLLoaderService::NumLoadersForTesting() const {
  return loader_receivers_.size() + disconnected_loaders_.size();
}

size_t KeepAliveURLLoaderService::NumDisconnectedLoadersForTesting() const {
  return disconnected_loaders_.size();
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
