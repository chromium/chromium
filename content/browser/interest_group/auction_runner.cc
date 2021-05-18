// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    GetAuctionServiceCallback get_auction_service,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
    auction_worklet::mojom::BrowserSignalsPtr browser_signals,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      std::move(get_auction_service), std::move(url_loader_factory),
      std::move(auction_config), std::move(bidders), std::move(browser_signals),
      std::move(callback)));
  instance->StartBidding();
  return instance;
}

AuctionRunner::AuctionRunner(
    GetAuctionServiceCallback get_auction_service,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
    auction_worklet::mojom::BrowserSignalsPtr browser_signals,
    RunAuctionCallback callback)
    : get_auction_service_(get_auction_service),
      url_loader_factory_(std::move(url_loader_factory)),
      auction_config_(std::move(auction_config)),
      bidders_(std::move(bidders)),
      browser_signals_(std::move(browser_signals)),
      callback_(std::move(callback)) {}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::StartBidding() {
  // Auctions are only run when there are bidders participating. As-is, and
  // empty bidder vector here would result in synchronously calling back into
  // the creator, which isn't allowed.
  DCHECK(!bidders_.empty());

  outstanding_bids_ = bidders_.size();
  bid_states_.resize(outstanding_bids_);

  for (int bid_index = 0; bid_index < outstanding_bids_; ++bid_index) {
    const auction_worklet::mojom::BiddingInterestGroupPtr& bidder =
        bidders_[bid_index];
    BidState* bid_state = &bid_states_[bid_index];
    bid_state->bidder = bidder.get();
    // TODO(morlovich): Straight skip if URL is missing.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    url_loader_factory_->Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());
    get_auction_service_.Run()->LoadBidderWorkletAndGenerateBid(
        bid_state->bidder_worklet.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory), bidder->Clone(),
        auction_config_->auction_signals, PerBuyerSignals(bid_state),
        browser_signals_->top_frame_origin, browser_signals_->seller,
        auction_start_time_,
        base::BindOnce(&AuctionRunner::OnGenerateBidComplete,
                       weak_ptr_factory_.GetWeakPtr(), bid_state));
    bid_state->bidder_worklet.set_disconnect_handler(
        base::BindOnce(&AuctionRunner::OnGenerateBidCrashed,
                       weak_ptr_factory_.GetWeakPtr(), bid_state));
  }

  // Also initiate the script fetch for the seller script.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  url_loader_factory_->Clone(
      url_loader_factory.InitWithNewPipeAndPassReceiver());
  get_auction_service_.Run()->LoadSellerWorklet(
      seller_worklet_.BindNewPipeAndPassReceiver(),
      std::move(url_loader_factory), auction_config_->decision_logic_url,
      base::BindOnce(&AuctionRunner::OnSellerWorkletLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
  // Fail auction if the seller worklet pipe is disconnected.
  seller_worklet_.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuction, weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnGenerateBidCrashed(BidState* state) {
  OnGenerateBidComplete(state, auction_worklet::mojom::BidderWorkletBidPtr(),
                        std::vector<std::string>{base::StrCat(
                            {state->bidder->group->bidding_url->spec(),
                             " crashed while trying to run generateBid()."})});
}

void AuctionRunner::OnGenerateBidComplete(
    BidState* state,
    auction_worklet::mojom::BidderWorkletBidPtr bid,
    const std::vector<std::string>& errors) {
  DCHECK(!state->bid_result);
  DCHECK_GT(outstanding_bids_, 0);

  --outstanding_bids_;

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // On failure, close the worklet pipe. On success, clear the disconnect
  // handler - crashed bidders only matters if it's the winning bidder that
  // crashed. That's checked for at the end of the auction.
  if (!bid) {
    state->bidder_worklet.reset();
  } else {
    state->bidder_worklet.set_disconnect_handler(base::OnceClosure());
  }

  state->bid_result = std::move(bid);

  if (ReadyToScore())
    ScoreOne();
}

void AuctionRunner::OnSellerWorkletLoaded(
    bool load_result,
    const std::vector<std::string>& errors) {
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  if (load_result) {
    seller_loaded_ = true;
    if (ReadyToScore())
      ScoreOne();
  } else {
    // Failed to load the seller/auction script --- nothing useful can be
    // done, so abort, possibly cancelling other fetches, so we don't waste
    // time.
    FailAuction();
  }
}

void AuctionRunner::ScoreOne() {
  size_t num_bidders = bid_states_.size();

  // Find next valid bid to score, if any.
  while (seller_considering_ < num_bidders) {
    BidState* bid_state = &bid_states_[seller_considering_];

    // Skip over bidders that produced no valid bid.
    if (!bid_state->bid_result) {
      ++seller_considering_;
      continue;
    }

    ScoreBid(bid_state);
    return;
  }

  DCHECK_EQ(seller_considering_, num_bidders);
  CompleteAuction();
}

void AuctionRunner::ScoreBid(const BidState* state) {
  seller_worklet_->ScoreAd(
      state->bid_result->ad, state->bid_result->bid, auction_config_.Clone(),
      browser_signals_->top_frame_origin, state->bidder->group->owner,
      AdRenderFingerprint(state),
      state->bid_result->bid_duration.InMilliseconds(),
      base::BindOnce(&AuctionRunner::OnBidScored,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnBidScored(double score,
                                const std::vector<std::string>& errors) {
  bid_states_[seller_considering_].seller_score = score;
  errors_.insert(errors_.end(), errors.begin(), errors.end());
  ++seller_considering_;
  ScoreOne();
}

std::string AuctionRunner::AdRenderFingerprint(const BidState* state) {
  // TODO(morlovich): "Eventually this fingerprint can be a hash of the ad web
  // bundle, but while rendering still uses the network, it should just be a
  // hash of the rendering URL."
  //
  return "#####";
}

absl::optional<std::string> AuctionRunner::PerBuyerSignals(
    const BidState* state) {
  if (auction_config_->per_buyer_signals.has_value()) {
    auto it = auction_config_->per_buyer_signals.value().find(
        state->bidder->group->owner);
    if (it != auction_config_->per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

void AuctionRunner::CompleteAuction() {
  double best_bid_score = 0.0;
  BidState* best_bid = nullptr;
  // TODO(morlovich): What if there is a tie?
  for (BidState& bid_state : bid_states_) {
    if (bid_state.seller_score > best_bid_score) {
      best_bid_score = bid_state.seller_score;
      best_bid = &bid_state;
    }
  }

  if (best_bid) {
    // Will eventually send a report to the seller and clean up `this`.
    ReportSellerResult(best_bid);
  } else {
    FailAuction();
  }
}

void AuctionRunner::ReportSellerResult(BidState* best_bid) {
  DCHECK(best_bid->bid_result);
  DCHECK_GT(best_bid->seller_score, 0);
  seller_worklet_->ReportResult(
      auction_config_.Clone(), browser_signals_->top_frame_origin,
      best_bid->bidder->group->owner, best_bid->bid_result->render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result->bid,
      best_bid->seller_score,
      base::BindOnce(&AuctionRunner::OnReportSellerResultComplete,
                     weak_ptr_factory_.GetWeakPtr(), best_bid));
}

void AuctionRunner::OnReportSellerResultComplete(
    BidState* best_bid,
    const absl::optional<std::string>& signals_for_winner,
    const absl::optional<GURL>& seller_report_url,
    const std::vector<std::string>& errors) {
  signals_for_winner_ = signals_for_winner;
  seller_report_url_ = seller_report_url;
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  ReportBidWin(best_bid);
}

void AuctionRunner::ReportBidWin(BidState* best_bid) {
  CHECK(best_bid->bid_result);
  std::string signals_for_winner_arg;
  // TODO(mmenke): It's unclear what should happen here if
  // `signals_for_winner_` is null. As-is, an empty string will result in the
  // BidderWorklet's ReportWin() method failing, since it's not valid JSON.
  if (signals_for_winner_)
    signals_for_winner_arg = *signals_for_winner_;

  // Fail the auction if the winning bidder process has crashed.
  //
  // TODO(mmenke): Be smarter about process crashes in general. Even without
  // the report URL, can display the ad and report to the seller (though will
  // need to think more about that case).
  //
  // TODO(mmenke): Make this FailAuction call (And likely others as well) add
  // a failure to `messages_`.
  if (!best_bid->bidder_worklet.is_connected()) {
    FailAuction();
    return;
  }

  best_bid->bidder_worklet->ReportWin(
      signals_for_winner_arg, best_bid->bid_result->render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result->bid,
      base::BindOnce(&AuctionRunner::OnReportBidWinComplete,
                     weak_ptr_factory_.GetWeakPtr(), best_bid));
  best_bid->bidder_worklet.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuction, weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnReportBidWinComplete(
    const BidState* best_bid,
    const absl::optional<GURL>& bidder_report_url,
    const std::vector<std::string>& errors) {
  bidder_report_url_ = bidder_report_url;
  errors_.insert(errors_.end(), errors.begin(), errors.end());
  ReportSuccess(best_bid);
}

void AuctionRunner::FailAuction() {
  DCHECK(callback_);
  ClosePipes();

  std::move(callback_).Run(GURL(), url::Origin(), std::string(), GURL(), GURL(),
                           errors_);
}

void AuctionRunner::ReportSuccess(const BidState* state) {
  DCHECK(callback_);
  DCHECK(state->bid_result);
  ClosePipes();

  std::move(callback_).Run(
      state->bid_result->render_url, state->bidder->group->owner,
      state->bidder->group->name,
      bidder_report_url_.has_value() ? *bidder_report_url_ : GURL(),
      seller_report_url_.has_value() ? *seller_report_url_ : GURL(), errors_);
}

void AuctionRunner::ClosePipes() {
  // This is needed in addition to closing worklet pipes in order to ignore
  // worklet creation callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  for (BidState& bid_state : bid_states_) {
    bid_state.bidder_worklet.reset();
  }
  seller_worklet_.reset();
}

}  // namespace content
