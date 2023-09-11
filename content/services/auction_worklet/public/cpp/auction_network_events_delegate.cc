// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"

#include <string>
#include <utility>

#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace auction_worklet {

MojoNetworkEventsDelegate::MojoNetworkEventsDelegate(
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        remote)
    : remote_(std::move(remote)) {}

MojoNetworkEventsDelegate::~MojoNetworkEventsDelegate() = default;

void MojoNetworkEventsDelegate::OnNetworkSendRequest(
    network::ResourceRequest& request) {
  request_id_ = request.devtools_request_id;
  remote_->OnNetworkSendRequest(request, base::TimeTicks::Now());
}

void MojoNetworkEventsDelegate::OnNetworkResponseReceived(
    const GURL& url,
    const network::mojom::URLResponseHead& head) {
  if (request_id_->empty()) {
    return;
  }
  remote_->OnNetworkResponseReceived(request_id_.value(), request_id_.value(),
                                     url, head.Clone());
}

void MojoNetworkEventsDelegate::OnNetworkRequestComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (request_id_->empty()) {
    return;
  }
  remote_->OnNetworkRequestComplete(request_id_.value(), status);
}

mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
CreateNewAuctionNetworkEventsHandlerRemote(
    const mojo::Remote<auction_worklet::mojom::AuctionNetworkEventsHandler>&
        remote) {
  // If we don't have a remote to clone, return a null remote.
  if (!remote) {
    return mojo::NullRemote();
  }

  mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
      auction_network_events_handler;
  remote->Clone(
      auction_network_events_handler.InitWithNewPipeAndPassReceiver());

  return auction_network_events_handler;
}

}  // namespace auction_worklet
