// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_runner.h"

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_worklet.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/seller_worklet.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace auction_worklet {

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

void AuctionRunner::CreateAndStart(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<mojom::BiddingInterestGroupPtr> bidders,
    mojom::BrowserSignalsPtr browser_signals,
    mojom::AuctionWorkletService::RunAuctionCallback callback) {
  AuctionRunner* instance = new AuctionRunner(
      std::move(url_loader_factory), std::move(auction_config),
      std::move(bidders), std::move(browser_signals), std::move(callback));
  instance->StartBidding();
}

AuctionRunner::AuctionRunner(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<mojom::BiddingInterestGroupPtr> bidders,
    mojom::BrowserSignalsPtr browser_signals,
    mojom::AuctionWorkletService::RunAuctionCallback callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      auction_config_(std::move(auction_config)),
      bidders_(std::move(bidders)),
      browser_signals_(std::move(browser_signals)),
      callback_(std::move(callback)) {}

AuctionRunner::~AuctionRunner() {
  DCHECK(callback_.is_null());
}

void AuctionRunner::StartBidding() {
  outstanding_bids_ = bidders_.size();
  bid_states_.resize(outstanding_bids_);

  for (int bid_index = 0; bid_index < outstanding_bids_; ++bid_index) {
    const mojom::BiddingInterestGroupPtr& bidder = bidders_[bid_index];
    BidState* bid_state = &bid_states_[bid_index];
    bid_state->bidder = bidder.get();
    // TODO(morlovich): Straight skip if URL is missing.
    bid_state->bidder_worklet = std::make_unique<BidderWorklet>(
        url_loader_factory_.get(), bidder->Clone(),
        auction_config_->auction_signals, PerBuyerSignals(bid_state),
        browser_signals_->top_frame_origin,
        browser_signals_->seller.Serialize(), auction_start_time_,
        &auction_v8_helper_,
        base::BindOnce(&AuctionRunner::OnGenerateBidComplete,
                       base::Unretained(this), bid_state));
  }

  // Also initiate the script fetch for the seller script.
  seller_worklet_ = std::make_unique<SellerWorklet>(
      url_loader_factory_.get(), auction_config_->decision_logic_url,
      &auction_v8_helper_,
      base::BindOnce(&AuctionRunner::OnSellerWorkletLoaded,
                     base::Unretained(this)));
}

void AuctionRunner::OnGenerateBidComplete(BidState* state,
                                          BidderWorklet::BidResult bid_result) {
  DCHECK(!state->bid_generate_complete);
  DCHECK_GT(outstanding_bids_, 0);

  --outstanding_bids_;

  errors_.insert(errors_.end(), bid_result.error_msgs.begin(),
                 bid_result.error_msgs.end());
  state->bid_generate_complete = true;
  state->bid_result = bid_result;

  if (ReadyToScore())
    ScoreOne();
}

void AuctionRunner::OnSellerWorkletLoaded(
    bool load_result,
    base::Optional<std::string> error_msg) {
  if (error_msg.has_value())
    errors_.push_back(std::move(error_msg).value());

  if (load_result) {
    seller_loaded_ = true;
    if (ReadyToScore())
      ScoreOne();
  } else {
    // Failed to load the seller/auction script --- nothing useful can be done,
    // so abort, possibly cancelling other fetches, so we don't waste time.
    FailAuction();
  }
}

void AuctionRunner::ScoreOne() {
  size_t num_bidders = bid_states_.size();

  // Skip over failed ones.
  while (seller_considering_ < num_bidders &&
         !bid_states_[seller_considering_].bid_result.success) {
    ++seller_considering_;
  }

  if (seller_considering_ < num_bidders) {
    BidState* bid_state = &bid_states_[seller_considering_];
    bid_state->score_result = ScoreBid(bid_state);
    ++seller_considering_;
    // If there is still some left, score them too, but let the event loop
    // roll first.
    if (seller_considering_ < num_bidders) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&AuctionRunner::ScoreOne, base::Unretained(this)));
      return;
    }
  }

  if (seller_considering_ == num_bidders)
    CompleteAuction();
}

SellerWorklet::ScoreResult AuctionRunner::ScoreBid(const BidState* state) {
  SellerWorklet::ScoreResult result = seller_worklet_->ScoreAd(
      state->bid_result.ad, state->bid_result.bid, *auction_config_,
      browser_signals_->top_frame_origin.host(), state->bidder->group->owner,
      AdRenderFingerprint(state), state->bid_result.bid_duration);
  if (result.error_msg.has_value())
    errors_.push_back(std::move(result.error_msg).value());
  return result;
}

std::string AuctionRunner::AdRenderFingerprint(const BidState* state) {
  // TODO(morlovich): "Eventually this fingerprint can be a hash of the ad web
  // bundle, but while rendering still uses the network, it should just be a
  // hash of the rendering URL."
  //
  return "#####";
}

base::Optional<std::string> AuctionRunner::PerBuyerSignals(
    const BidState* state) {
  if (auction_config_->per_buyer_signals.has_value()) {
    auto it = auction_config_->per_buyer_signals.value().find(
        state->bidder->group->owner);
    if (it != auction_config_->per_buyer_signals.value().end())
      return it->second;
  }
  return base::nullopt;
}

void AuctionRunner::CompleteAuction() {
  double best_bid_score = 0.0;
  const BidState* best_bid = nullptr;
  // TODO(morlovich): What if there is a tie?
  for (const BidState& bid_state : bid_states_) {
    if (bid_state.score_result.score > best_bid_score) {
      best_bid_score = bid_state.score_result.score;
      best_bid = &bid_state;
    }
  }

  if (best_bid) {
    SellerWorklet::Report seller_report = ReportSellerResult(best_bid);
    BidderWorklet::ReportWinResult bidder_report =
        ReportBidWin(best_bid, seller_report);
    ReportSuccess(best_bid, bidder_report, seller_report);
  } else {
    FailAuction();
  }
}

SellerWorklet::Report AuctionRunner::ReportSellerResult(
    const BidState* best_bid) {
  SellerWorklet::Report result = seller_worklet_->ReportResult(
      *auction_config_, browser_signals_->top_frame_origin.host(),
      best_bid->bidder->group->owner, best_bid->bid_result.render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result.bid,
      best_bid->score_result.score);
  if (result.error_msg.has_value())
    errors_.push_back(std::move(result.error_msg).value());
  return result;
}

BidderWorklet::ReportWinResult AuctionRunner::ReportBidWin(
    const BidState* best_bid,
    const SellerWorklet::Report& seller_report) {
  BidderWorklet::ReportWinResult result = best_bid->bidder_worklet->ReportWin(
      seller_report.signals_for_winner, best_bid->bid_result.render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result.bid);
  if (result.error_msg.has_value())
    errors_.push_back(std::move(result.error_msg).value());
  return result;
}

void AuctionRunner::FailAuction() {
  std::move(callback_).Run(
      GURL(), url::Origin(), std::string(),
      mojom::WinningBidderReport::New(false /* success */, GURL()),
      mojom::SellerReport::New(false /* success */, "", GURL()), errors_);
  delete this;
}

void AuctionRunner::ReportSuccess(
    const BidState* state,
    const BidderWorklet::ReportWinResult& bidder_report,
    const SellerWorklet::Report& seller_report) {
  std::move(callback_).Run(
      state->bid_result.render_url, state->bidder->group->owner,
      state->bidder->group->name,
      mojom::WinningBidderReport::New(bidder_report.success,
                                      bidder_report.report_url),
      mojom::SellerReport::New(seller_report.success,
                               seller_report.signals_for_winner,
                               seller_report.report_url),
      errors_);
  delete this;
}

}  // namespace auction_worklet
