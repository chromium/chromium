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
#include "content/services/auction_worklet/trusted_bidding_signals.h"
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
        url_loader_factory_.get(), bidder->group->bidding_url.value_or(GURL()),
        &auction_v8_helper_,
        base::BindOnce(&AuctionRunner::OnBidderScriptLoaded,
                       base::Unretained(this), bid_state));
    if (bidder->group->trusted_bidding_signals_url.has_value()) {
      bid_state->trusted_bidding_signals =
          std::make_unique<TrustedBiddingSignals>(
              url_loader_factory_.get(),
              bidder->group->trusted_bidding_signals_keys.has_value()
                  ? bidder->group->trusted_bidding_signals_keys.value()
                  : std::vector<std::string>(),
              browser_signals_->top_frame_origin.host(),
              bidder->group->trusted_bidding_signals_url.value_or(GURL()),
              &auction_v8_helper_,
              base::BindOnce(&AuctionRunner::OnTrustedSignalsLoaded,
                             base::Unretained(this), bid_state));
    } else {
      OnTrustedSignalsLoaded(bid_state, false);
    }
  }

  // Also initiate the script fetch for the seller script.
  seller_worklet_ = std::make_unique<SellerWorklet>(
      url_loader_factory_.get(), auction_config_->decision_logic_url,
      &auction_v8_helper_,
      base::BindOnce(&AuctionRunner::OnSellerWorkletLoaded,
                     base::Unretained(this)));
}

void AuctionRunner::OnBidderScriptLoaded(BidState* state, bool load_result) {
  DCHECK(!state->bidder_script_loaded);
  DCHECK(!state->failed);
  state->bidder_script_loaded = true;
  if (!load_result) {
    state->failed = true;

    // No point in waiting for trusted signals (if any) if we don't have a
    // bidder script that looks at them.
    state->trusted_bidding_signals.reset();
    state->trusted_signals_loaded = true;
  }

  MaybeRunBid(state);
}

void AuctionRunner::OnTrustedSignalsLoaded(BidState* state, bool load_result) {
  DCHECK(!state->trusted_signals_loaded);
  state->trusted_signals_loaded = true;
  if (!load_result)
    state->trusted_bidding_signals.reset();

  MaybeRunBid(state);
}

void AuctionRunner::MaybeRunBid(BidState* state) {
  if (!state->trusted_signals_loaded || !state->bidder_script_loaded)
    return;

  --outstanding_bids_;
  if (!state->failed)
    RunBid(state);

  if (ReadyToScore())
    ScoreOne();
}

void AuctionRunner::RunBid(BidState* state) {
  base::TimeTicks start = base::TimeTicks::Now();
  state->bid_result = state->bidder_worklet->GenerateBid(
      *state->bidder->group, auction_config_->auction_signals,
      PerBuyerSignals(state),
      state->bidder->group->trusted_bidding_signals_keys.value_or(
          std::vector<std::string>()),
      state->trusted_bidding_signals.get(),
      browser_signals_->top_frame_origin.host(),
      browser_signals_->seller.Serialize(), state->bidder->signals->join_count,
      state->bidder->signals->bid_count, state->bidder->signals->prev_wins,
      auction_start_time_);
  state->bid_duration = base::TimeTicks::Now() - start;
}

void AuctionRunner::OnSellerWorkletLoaded(bool load_result) {
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
         (bid_states_[seller_considering_].failed ||
          !bid_states_[seller_considering_].bid_result.success)) {
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
  return seller_worklet_->ScoreAd(
      state->bid_result.ad, state->bid_result.bid, *auction_config_,
      browser_signals_->top_frame_origin.host(), state->bidder->group->owner,
      AdRenderFingerprint(state), state->bid_duration);
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
  return seller_worklet_->ReportResult(
      *auction_config_, browser_signals_->top_frame_origin.host(),
      best_bid->bidder->group->owner, best_bid->bid_result.render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result.bid,
      best_bid->score_result.score);
}

BidderWorklet::ReportWinResult AuctionRunner::ReportBidWin(
    const BidState* best_bid,
    const SellerWorklet::Report& seller_report) {
  return best_bid->bidder_worklet->ReportWin(
      auction_config_->auction_signals, PerBuyerSignals(best_bid),
      seller_report.signals_for_winner,
      browser_signals_->top_frame_origin.host(), best_bid->bidder->group->owner,
      best_bid->bidder->group->name, best_bid->bid_result.render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result.bid);
}

void AuctionRunner::FailAuction() {
  std::move(callback_).Run(
      GURL(), url::Origin(), std::string(),
      mojom::WinningBidderReport::New(false /* success */, GURL()),
      mojom::SellerReport::New(false /* success */, "", GURL()));
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
                               seller_report.report_url));
  delete this;
}

}  // namespace auction_worklet
