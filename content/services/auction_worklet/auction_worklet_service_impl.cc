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
    const GURL& script_source_url,
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto bidder_worklet = std::make_unique<BidderWorklet>(
      auction_v8_helper_, pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), script_source_url, wasm_helper_url,
      trusted_bidding_signals_url, top_window_origin,
      has_experiment_group_id ? absl::make_optional(experiment_group_id)
                              : absl::nullopt);
  auto* bidder_worklet_ptr = bidder_worklet.get();

  mojo::ReceiverId receiver_id = bidder_worklets_.Add(
      std::move(bidder_worklet), std::move(bidder_worklet_receiver));

  bidder_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectBidderWorklet,
                     base::Unretained(this), receiver_id));
}

void AuctionWorkletServiceImpl::LoadSellerWorklet(
    mojo::PendingReceiver<mojom::SellerWorklet> seller_worklet_receiver,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin,
    bool has_experiment_group_id,
    uint16_t experiment_group_id) {
  auto seller_worklet = std::make_unique<SellerWorklet>(
      auction_v8_helper_, pause_for_debugger_on_start,
      std::move(pending_url_loader_factory), decision_logic_url,
      trusted_scoring_signals_url, top_window_origin,
      has_experiment_group_id ? absl::make_optional(experiment_group_id)
                              : absl::nullopt);
  auto* seller_worklet_ptr = seller_worklet.get();

  mojo::ReceiverId receiver_id = seller_worklets_.Add(
      std::move(seller_worklet), std::move(seller_worklet_receiver));

  seller_worklet_ptr->set_close_pipe_callback(
      base::BindOnce(&AuctionWorkletServiceImpl::DisconnectSellerWorklet,
                     base::Unretained(this), receiver_id));
}

void AuctionWorkletServiceImpl::DisconnectSellerWorklet(
    mojo::ReceiverId receiver_id,
    const std::string& reason) {
  seller_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                    reason);
}

void AuctionWorkletServiceImpl::DisconnectBidderWorklet(
    mojo::ReceiverId receiver_id,
    const std::string& reason) {
  bidder_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                    reason);
}

}  // namespace auction_worklet
