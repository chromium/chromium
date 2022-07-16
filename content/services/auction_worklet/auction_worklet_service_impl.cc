// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_worklet_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace auction_worklet {

AuctionWorkletServiceImpl::AuctionWorkletServiceImpl(
    mojo::PendingReceiver<mojom::AuctionWorkletService> receiver)
    : receiver_(this, std::move(receiver)),
      auction_v8_helper_(
          AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())) {}

AuctionWorkletServiceImpl::~AuctionWorkletServiceImpl() = default;

void AuctionWorkletServiceImpl::LoadBidderWorklet(
    mojo::PendingReceiver<mojom::BidderWorklet> bidder_worklet_receiver,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    mojom::BiddingInterestGroupPtr bidding_interest_group) {
  bidder_worklets_.Add(std::make_unique<BidderWorklet>(
                           auction_v8_helper_, pause_for_debugger_on_start,
                           std::move(pending_url_loader_factory),
                           std::move(bidding_interest_group)),
                       std::move(bidder_worklet_receiver));
}

void AuctionWorkletServiceImpl::LoadSellerWorklet(
    mojo::PendingReceiver<mojom::SellerWorklet> seller_worklet_receiver,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    LoadSellerWorkletCallback load_seller_worklet_callback) {
  seller_worklets_.Add(
      std::make_unique<SellerWorklet>(
          auction_v8_helper_, pause_for_debugger_on_start,
          std::move(pending_url_loader_factory), script_source_url,
          std::move(load_seller_worklet_callback)),
      std::move(seller_worklet_receiver));
}

}  // namespace auction_worklet
