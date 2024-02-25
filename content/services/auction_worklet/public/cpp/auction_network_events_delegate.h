// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_NETWORK_EVENTS_DELEGATE_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_NETWORK_EVENTS_DELEGATE_H_

#include <string>
#include <utility>

#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace auction_worklet {

// Implementation of AuctionDownloader::NetworkEventsDelegate.
// This handles how network requests get sent over through mojo
// and logged to devtools.
class CONTENT_EXPORT MojoNetworkEventsDelegate
    : public AuctionDownloader::NetworkEventsDelegate {
 public:
  explicit MojoNetworkEventsDelegate(
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          remote);
  ~MojoNetworkEventsDelegate() override;

  void OnNetworkSendRequest(network::ResourceRequest& request) override;
  void OnNetworkResponseReceived(
      const GURL& url,
      const network::mojom::URLResponseHead& head) override;

  void OnNetworkRequestComplete(
      const network::URLLoaderCompletionStatus& status) override;

 private:
  mojo::Remote<auction_worklet::mojom::AuctionNetworkEventsHandler> remote_;
  std::optional<std::string> request_id_;
};
// Handles the creation of a new remote and binds it's receiver to
// the same implementation as the remote passed in.
mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
CreateNewAuctionNetworkEventsHandlerRemote(
    const mojo::Remote<auction_worklet::mojom::AuctionNetworkEventsHandler>&
        remote);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_AUCTION_NETWORK_EVENTS_DELEGATE_H_
