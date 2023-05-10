// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

namespace content {

class PolicyContainerHost;

// A service that stores bound SharedURLLoaderFactory mojo pipes and the loaders
// they have created to load fetch keepalive requests.
//
// A fetch keepalive request is originated from a JS call to
// `fetch(..., {keepalive: true})` or `navigator.sendBeacon()`. A renderer can
// ask this service to handle such request by using a remote of
// mojom::URLLoaderFactory bound to this service by `BindFactory()`, which also
// binds RenderFrameHostImpl-specific context with every receiver.
//
// Calling the remote `CreateLoaderAndStart()` will create a
// `KeepAliveURLLoader` in browser. The service is also responsible for keeping
// these loaders in `loader_receivers_` until the corresponding request
// completes or fails.
//
// Handling keepalive requests in this service allows a request to continue even
// if a renderer unloads before completion, i.e. the request is "keepalive".
//
// This service is created and stored in every `StoragePartitionImpl` instance.
//
// Design Doc:
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class CONTENT_EXPORT KeepAliveURLLoaderService {
 public:
  explicit KeepAliveURLLoaderService();
  ~KeepAliveURLLoaderService();

  // Not Copyable.
  KeepAliveURLLoaderService(const KeepAliveURLLoaderService&) = delete;
  KeepAliveURLLoaderService& operator=(const KeepAliveURLLoaderService&) =
      delete;

  // Binds the pending `receiver` with this service, using `pending_factory`.
  //
  // The remote of `receiver` can be passed to another process, i.e. renderer,
  // to handle fetch keepalive requests.
  //
  // `policy_container_host` is the policy host of the frame that is going to
  // use the remote of `receiver` to load requests. It must not be null.
  void BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      scoped_refptr<network::SharedURLLoaderFactory>
          subresource_proxying_factory_bundle,
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // For testing only:
  size_t NumLoadersForTesting() const;
  size_t NumDisconnectedLoadersForTesting() const;
  void SetLoaderObserverForTesting(
      scoped_refptr<KeepAliveURLLoader::TestObserver> observer);

 private:
  class KeepAliveURLLoaderFactory;

  // Handles every disconnection notification for `loader_receivers_`.
  void OnLoaderDisconnected();

  // Removes the KeepAliveURLLoader kept by this service, either from
  // `loader_receivers_` or `disconnected_loaders_`.
  void RemoveLoader(mojo::ReceiverId loader_receiver_id);

  // Many-to-one mojo receiver of URLLoaderFactory.
  std::unique_ptr<KeepAliveURLLoaderFactory> factory_;

  // Holds all the KeepAliveURLLoader connected with remotes in renderers.
  // Each of them corresponds to the handling of one pending keepalive request.
  // Once a receiver is disconnected, its context should be moved to
  // `disconnected_loaders_`.
  mojo::ReceiverSet<network::mojom::URLLoader,
                    std::unique_ptr<network::mojom::URLLoader>>
      loader_receivers_;

  // Holds all the KeepAliveURLLoader that has been disconnected from renderers.
  // They should be kept alive until the request completes or fails.
  // The key is the mojo::ReceiverId assigned by `loader_receivers_`.
  std::map<mojo::ReceiverId, std::unique_ptr<network::mojom::URLLoader>>
      disconnected_loaders_;

  // For testing only:
  // Not owned.
  scoped_refptr<KeepAliveURLLoader::TestObserver> loader_test_observer_ =
      nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
