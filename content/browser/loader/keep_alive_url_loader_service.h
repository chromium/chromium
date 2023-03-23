// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_

#include <map>
#include <memory>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

namespace content {

// A service that stores bound SharedURLLoaderFactory mojo pipes. Every remote
// of the pipes can be used to create a URLLoader that loads fetch keepalive
// requests. The service is responsible for keeping the loaders in
// `loader_receivers_`.
//
// A renderer can ask this service to handle `fetch(..., {keepalive: true})` or
// `navigator.sendBeacon()` requests by using a remote of URLLoaderFactory bound
// to this service by `BindFactory()`,
//
// Handling keepalive requests in this service allows a request to continue even
// if a renderer unloads before completion, i.e. the request is "keepalive".
//
// Design Doc:
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit#
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
  void BindFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);

  // For testing only:
  size_t NumLoadersForTesting() const;
  size_t NumDisconnectedLoadersForTesting() const;

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_SERVICE_H_
