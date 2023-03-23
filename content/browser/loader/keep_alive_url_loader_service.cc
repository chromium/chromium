// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_thread.h"
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
struct BindContext {
  explicit BindContext(scoped_refptr<network::SharedURLLoaderFactory> factory)
      : factory(factory) {}

  explicit BindContext(const std::unique_ptr<BindContext>& other)
      : factory(other->factory) {}

  ~BindContext() = default;

  // A refptr to the factory to use for the requests initiated from this
  // context.
  scoped_refptr<network::SharedURLLoaderFactory> factory;
  // This must be the last member.
  base::WeakPtrFactory<BindContext> weak_ptr_factory{this};
};
}  // namespace

// A URLLoaderFactory to handle fetch keepalive requests.
//
// This factory can handle requests from multiple remotes of URLLoaderFactory.
// Users should call `BindFactory()` first to register a pending receiver with
// this factory.
//
// On requested by a remote, i.e. calling
// `network::mojom::URLLoaderFactory::CreateLoaderAndStart()`, this factory will
// create a KeepAliveURLLoader to load a keepalive request. The loader is held
// by the `KeepAliveURLLoaderService` owning this factory.
//
// This factory must be run in the browser process.
//
// See the "Implementation Details" section of the design doc
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit#
class KeepAliveURLLoaderService::KeepAliveURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit KeepAliveURLLoaderFactory(KeepAliveURLLoaderService* service)
      : service_(service) {
    DCHECK(service_);
  }
  ~KeepAliveURLLoaderFactory() override = default;

  // Not copyable.
  KeepAliveURLLoaderFactory(const KeepAliveURLLoaderFactory&) = delete;
  KeepAliveURLLoaderFactory& operator=(const KeepAliveURLLoaderFactory&) =
      delete;

  // Creates a `BindContext` to hold a refptr to
  // network::SharedURLLoaderFactory, which is constructed with
  // `pending_factory`, and then bound with `receiver`.
  void BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);

  // `network::mojom::URLLoaderFactory` overrides:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request_in,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  // Guaranteed to exist, as `service_` owns this object.
  raw_ptr<KeepAliveURLLoaderService> service_;

  // Receives `network::mojom::URLLoaderFactory` requests from renderers.
  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    std::unique_ptr<BindContext>>
      loader_factory_receivers_;
};

void KeepAliveURLLoaderService::KeepAliveURLLoaderFactory::BindFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("loading", "KeepAliveURLLoaderFactory::BindFactory");

  auto factory_bundle =
      network::SharedURLLoaderFactory::Create(std::move(pending_factory));
  loader_factory_receivers_.Add(this, std::move(receiver),
                                std::make_unique<BindContext>(factory_bundle));
}

void KeepAliveURLLoaderService::KeepAliveURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoaderFactory::CreateLoaderAndStart",
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

  // Creates a new KeepAliveURLLoader from the current context.
  const std::unique_ptr<BindContext>& current_context =
      loader_factory_receivers_.current_context();
  // Passes in the pending remote of `client` from a renderer so that `loader`
  // can forward response back to the renderer.
  auto loader = std::make_unique<KeepAliveURLLoader>(
      request_id, options, resource_request, std::move(client),
      traffic_annotation, current_context->factory,
      base::PassKey<KeepAliveURLLoaderService>());
  // Adds a new receiver to the set, binding the pending `receiver` from a
  // renderer to `raw_loader` with the context `loader` to handle URL requests.
  auto* raw_loader = loader.get();
  auto receiver_id = service_->loader_receivers_.Add(
      raw_loader, std::move(receiver), std::move(loader));
  raw_loader->set_on_delete_callback(
      base::BindOnce(&KeepAliveURLLoaderService::RemoveLoader,
                     base::Unretained(service_), receiver_id));
}

void KeepAliveURLLoaderService::KeepAliveURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  loader_factory_receivers_.Add(
      this, std::move(receiver),
      std::make_unique<BindContext>(
          loader_factory_receivers_.current_context()));
}

KeepAliveURLLoaderService::KeepAliveURLLoaderService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  factory_->BindFactory(std::move(receiver), std::move(pending_factory));
}

void KeepAliveURLLoaderService::OnLoaderDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto disconnected_loader_receiver_id = loader_receivers_.current_receiver();
  TRACE_EVENT1("loading", "KeepAliveURLLoaderService::OnLoaderDisconnected",
               "loader_id", disconnected_loader_receiver_id);

  // The context of `disconnected_loader_receiver_id`, an KeepAliveURLLoader
  // object, has been removed from `loader_receivers_`, but it has to stay alive
  // to handle subsequent updates from network service.
  disconnected_loaders_.emplace(disconnected_loader_receiver_id,
                                std::move(loader_receivers_.current_context()));
}

void KeepAliveURLLoaderService::RemoveLoader(
    mojo::ReceiverId loader_receiver_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoaderService::RemoveLoader",
               "loader_id", loader_receiver_id);

  loader_receivers_.Remove(loader_receiver_id);
  disconnected_loaders_.erase(loader_receiver_id);
}

size_t KeepAliveURLLoaderService::NumLoadersForTesting() const {
  return loader_receivers_.size() + disconnected_loaders_.size();
}

size_t KeepAliveURLLoaderService::NumDisconnectedLoadersForTesting() const {
  return disconnected_loaders_.size();
}

}  // namespace content
