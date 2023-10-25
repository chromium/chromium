// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"

namespace content {

class BrowserContext;
class PolicyContainerHost;

// A service that stores bound SharedURLLoaderFactory mojo pipes from renderers
// of the same storage partition, and the intermediate URLLoader receivers, i.e.
// KeepAliveURLLoader, they have created to load fetch keepalive requests.
//
// A fetch keepalive request is originated from a JS call to
// `fetch(..., {keepalive: true})` or `navigator.sendBeacon()`. A renderer can
// ask this service to handle such request by using a remote of
// mojom::URLLoaderFactory bound to this service by `BindFactory()`, which also
// binds RenderFrameHostImpl-specific context with every receiver.
//
// Calling the remote `CreateLoaderAndStart()` of a factory will create a
// `KeepAliveURLLoader` here in browser. The service is responsible for keeping
// these loaders in `loader_receivers_` until the corresponding request
// completes or fails.
//
// Handling keepalive requests in this service allows a request to continue even
// if a renderer unloads before completion, i.e. the request is "keepalive",
// without needing the renderer to stay extra longer than the necessary time.
//
// This service is created and stored in every `StoragePartitionImpl` instance.
// Hence, its lifetime is the same as the owner StoragePartition for a partition
// domain, which should be generally longer than any of the renderers spawned
// from the partition domain.
//
// Design Doc:
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class CONTENT_EXPORT KeepAliveURLLoaderService {
 public:
  // `browser_context` owns the StoragePartition creating the instance of this
  // service. It must not be null and surpass the lifetime of this service.
  explicit KeepAliveURLLoaderService(BrowserContext* browser_context);
  ~KeepAliveURLLoaderService();

  // Not Copyable.
  KeepAliveURLLoaderService(const KeepAliveURLLoaderService&) = delete;
  KeepAliveURLLoaderService& operator=(const KeepAliveURLLoaderService&) =
      delete;

  // Binds the pending `receiver` with this service, using
  // `subresource_proxying_factory_bundle`.
  //
  // The remote of `receiver` can be passed to another process, i.e. renderer,
  // from which to create new fetch keepalive requests.
  //
  // `policy_container_host` is the policy host of the requester frame going to
  // use the remote of `receiver` to load requests. It must not be null.
  void BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // Binds the pending FetchLaterLoaderFactory `receiver` with this service,
  // which uses `factory` to load FetchLater URL requests.
  // See also `BindFactory()` for other parameters.
  void BindFetchLaterLoaderFactory(
      mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>
          receiver,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // Called when the `browser_context_` that owns this instance is shutting
  // down.
  void Shutdown();

  // For testing only:
  size_t NumLoadersForTesting() const;
  size_t NumDisconnectedLoadersForTesting() const;
  void SetLoaderObserverForTesting(
      scoped_refptr<KeepAliveURLLoader::TestObserver> observer);
  void SetURLLoaderThrottlesGetterForTesting(
      KeepAliveURLLoader::URLLoaderThrottlesGetter
          url_loader_throttles_getter_for_testing);

 private:
  template <typename Interface,
            template <typename>
            class PendingReceiverType,
            template <typename, typename>
            class ReceiverSetType>
  class KeepAliveURLLoaderFactoriesBase;
  class KeepAliveURLLoaderFactories;
  class FetchLaterLoaderFactories;

  // Handles every disconnection notification for `loader_receivers_`.
  void OnLoaderDisconnected();

  // Removes the KeepAliveURLLoader kept by this service, either from
  // `loader_receivers_` or `disconnected_loaders_`.
  void RemoveLoader(mojo::ReceiverId loader_receiver_id);

  // The browsing session that owns this instance of the service.
  const raw_ptr<BrowserContext> browser_context_;

  // Many-to-one mojo receiver of URLLoaderFactory for Fetch keepalive requests.
  std::unique_ptr<KeepAliveURLLoaderFactories> url_loader_factories_;

  // Many-to-one mojo receiver of FetchLaterLoaderFactory for FetchLater
  // keepalive requests.
  std::unique_ptr<FetchLaterLoaderFactories> fetch_later_loader_factories_;

  // For testing only:
  // Not owned.
  scoped_refptr<KeepAliveURLLoader::TestObserver> loader_test_observer_ =
      nullptr;
  KeepAliveURLLoader::URLLoaderThrottlesGetter
      url_loader_throttles_getter_for_testing_ = base::NullCallback();
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
