// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_worklet_service_impl.h"

#include <string>
#include <utility>

#include "content/services/auction_worklet/auction_runner.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace auction_worklet {

AuctionWorkletServiceImpl::AuctionWorkletServiceImpl(
    mojo::PendingReceiver<mojom::AuctionWorkletService> receiver)
    : receiver_(this, std::move(receiver)) {}

AuctionWorkletServiceImpl::~AuctionWorkletServiceImpl() = default;

void AuctionWorkletServiceImpl::RunAuction(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<mojom::BiddingInterestGroupPtr> bidders,
    mojom::BrowserSignalsPtr browser_signals,
    mojom::AuctionWorkletService::RunAuctionCallback callback) {
  AuctionRunner::CreateAndStart(
      std::move(url_loader_factory), std::move(auction_config),
      std::move(bidders), std::move(browser_signals), std::move(callback));
}

}  // namespace auction_worklet
