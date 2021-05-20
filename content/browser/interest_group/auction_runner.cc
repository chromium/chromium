// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "net/base/escape.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// All URLs received from worklets must be valid HTTPS URLs. It's up to callers
// to call ReportBadMessage() on invalid URLs.
bool IsUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

// Validates that `bid` is valid and, if it is, returns the InterestGroupAd
// corresponding to the bid. Returns nullptr and calls ReportBadMessage() if
// not. If non-null, the returned pointer will point at the winning
// blink::mojom::InterestGroupAd within `bid`.
blink::mojom::InterestGroupAd* ValidateBidAndGetAd(
    const auction_worklet::mojom::BidderWorkletBid& bid,
    const blink::mojom::InterestGroup& interest_group) {
  if (bid.bid <= 0 || std::isnan(bid.bid) || !std::isfinite(bid.bid)) {
    mojo::ReportBadMessage("Invalid bid value");
    return nullptr;
  }

  if (bid.bid_duration < base::TimeDelta()) {
    mojo::ReportBadMessage("Invalid bid duration");
    return nullptr;
  }

  // This should be a subset of the next case, but best to be careful.
  if (!IsUrlValid(bid.render_url)) {
    mojo::ReportBadMessage("Invalid bid render URL");
    return nullptr;
  }

  // Reject URLs not listed in the interest group.
  for (const auto& ad : interest_group.ads.value()) {
    if (ad->render_url == bid.render_url) {
      return ad.get();
    }
  }

  mojo::ReportBadMessage("Bid render URL must be an ad URL");
  return nullptr;
}

}  // namespace

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    Delegate* delegate,
    InterestGroupManager* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<url::Origin> filtered_buyers,
    auction_worklet::mojom::BrowserSignalsPtr browser_signals,
    const url::Origin& frame_origin,
    RunAuctionCallback callback) {
  DCHECK(!filtered_buyers.empty());
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      delegate, interest_group_manager, std::move(auction_config),
      std::move(filtered_buyers), std::move(browser_signals), frame_origin,
      std::move(callback)));
  instance->ReadNextInterestGroup();
  return instance;
}

AuctionRunner::AuctionRunner(
    Delegate* delegate,
    InterestGroupManager* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<url::Origin> filtered_buyers,
    auction_worklet::mojom::BrowserSignalsPtr browser_signals,
    const url::Origin& frame_origin,
    RunAuctionCallback callback)
    : delegate_(delegate),
      interest_group_manager_(interest_group_manager),
      auction_config_(std::move(auction_config)),
      pending_buyers_(std::move(filtered_buyers)),
      browser_signals_(std::move(browser_signals)),
      frame_origin_(frame_origin),
      callback_(std::move(callback)) {}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::ReadNextInterestGroup() {
  DCHECK_LT(next_pending_buyer_, pending_buyers_.size());

  interest_group_manager_->GetInterestGroupsForOwner(
      pending_buyers_[next_pending_buyer_],
      base::BindOnce(&AuctionRunner::OnInterestGroupRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnInterestGroupRead(
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
        interest_groups) {
  for (auto bidder = std::make_move_iterator(interest_groups.begin());
       bidder != std::make_move_iterator(interest_groups.end()); ++bidder) {
    bid_states_.emplace_back(BidState());
    bid_states_.back().bidder = std::move(*bidder);
  }
  next_pending_buyer_++;

  // If more buyers in the queue, load the next one.
  if (next_pending_buyer_ < pending_buyers_.size()) {
    ReadNextInterestGroup();
    return;
  }

  // Pending buyers are no longer needed.
  pending_buyers_.clear();

  // If no interest groups were found, end the auction without a winner.
  if (bid_states_.empty()) {
    FailAuction();
    return;
  }

  StartBidding();
}

void AuctionRunner::StartBidding() {
  // Auctions are only run when there are bidders participating. As-is, and
  // empty bidder vector here would result in synchronously calling back into
  // the creator, which isn't allowed.
  DCHECK(!bid_states_.empty());

  outstanding_bids_ = bid_states_.size();

  for (auto& bid_state : bid_states_) {
    auction_worklet::mojom::BiddingInterestGroup* bidder =
        bid_state.bidder.get();

    // Assemble list of URLs the bidder can request.

    // TODO(mmenke): This largely duplicates logic in the auction worklet
    // service. Avoid duplicating code.
    absl::optional<GURL> trusted_bidding_signals_full_url;
    if (bid_state.bidder->group->trusted_bidding_signals_url &&
        bid_state.bidder->group->trusted_bidding_signals_keys) {
      std::string query_params =
          "hostname=" + net::EscapeQueryParamValue(
                            browser_signals_->top_frame_origin.host(), true);
      query_params += "&keys=";
      bool first_key = true;
      for (const auto& key :
           *bid_state.bidder->group->trusted_bidding_signals_keys) {
        if (first_key) {
          first_key = false;
        } else {
          query_params.append(",");
        }
        query_params.append(net::EscapeQueryParamValue(key, true));
      }

      GURL::Replacements replacements;
      replacements.SetQueryStr(query_params);
      trusted_bidding_signals_full_url =
          bid_state.bidder->group->trusted_bidding_signals_url
              ->ReplaceComponents(replacements);
    }

    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    bid_state.url_loader_factory_ =
        std::make_unique<AuctionURLLoaderFactoryProxy>(
            url_loader_factory.InitWithNewPipeAndPassReceiver(),
            base::BindRepeating(&Delegate::GetTrustedURLLoaderFactory,
                                base::Unretained(delegate_)),
            frame_origin_, false /* use_cors */,
            bid_state.bidder->group->bidding_url.value_or(GURL()),
            trusted_bidding_signals_full_url);

    delegate_->GetWorkletService()->LoadBidderWorkletAndGenerateBid(
        bid_state.bidder_worklet.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory), bidder->Clone(),
        auction_config_->auction_signals, PerBuyerSignals(&bid_state),
        browser_signals_->top_frame_origin, browser_signals_->seller,
        auction_start_time_,
        base::BindOnce(&AuctionRunner::OnGenerateBidComplete,
                       weak_ptr_factory_.GetWeakPtr(), &bid_state));
    bid_state.bidder_worklet.set_disconnect_handler(
        base::BindOnce(&AuctionRunner::OnGenerateBidCrashed,
                       weak_ptr_factory_.GetWeakPtr(), &bid_state));
  }

  // Also initiate the script fetch for the seller script.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  seller_url_loader_factory_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      url_loader_factory.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&Delegate::GetFrameURLLoaderFactory,
                          base::Unretained(delegate_)),
      frame_origin_, true /* use_cors */, auction_config_->decision_logic_url);
  delegate_->GetWorkletService()->LoadSellerWorklet(
      seller_worklet_.BindNewPipeAndPassReceiver(),
      std::move(url_loader_factory), auction_config_->decision_logic_url,
      base::BindOnce(&AuctionRunner::OnSellerWorkletLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
  // Fail auction if the seller worklet pipe is disconnected.
  seller_worklet_.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuctionWithError, weak_ptr_factory_.GetWeakPtr(),
      base::StrCat({auction_config_->decision_logic_url.spec(), " crashed."})));
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

  // Ignore invalid bids.
  if (bid) {
    state->bid_ad = ValidateBidAndGetAd(*bid, *state->bidder->group);
    if (!state->bid_ad)
      bid.reset();
  }

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

  // Find the best bid, if any, and record the bid of all bidders that made a
  // bid. Record bidders at this point because the auction has successfully
  // completed, and need to record bids even in the case the seller rejected all
  // bids.
  //
  // TODO(morlovich): What if there is a tie?
  for (BidState& bid_state : bid_states_) {
    if (bid_state.bid_result) {
      interest_group_manager_->RecordInterestGroupBid(
          bid_state.bidder->group->owner, bid_state.bidder->group->name);
      if (bid_state.seller_score > best_bid_score) {
        best_bid_score = bid_state.seller_score;
        best_bid = &bid_state;
      }
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

  absl::optional<GURL> opt_bidder_report_url;
  if (seller_report_url && !IsUrlValid(*seller_report_url)) {
    mojo::ReportBadMessage("Invalid seller report URL");
    FailAuction();
    return;
  }

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
  if (!best_bid->bidder_worklet.is_connected()) {
    FailAuctionWithError(
        base::StrCat({best_bid->bidder->group->bidding_url->spec(),
                      " crashed while idle."}));
    return;
  }

  best_bid->bidder_worklet->ReportWin(
      signals_for_winner_arg, best_bid->bid_result->render_url,
      AdRenderFingerprint(best_bid), best_bid->bid_result->bid,
      base::BindOnce(&AuctionRunner::OnReportBidWinComplete,
                     weak_ptr_factory_.GetWeakPtr(), best_bid));
  best_bid->bidder_worklet.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuctionWithError, weak_ptr_factory_.GetWeakPtr(),
      base::StrCat({best_bid->bidder->group->bidding_url->spec(),
                    " crashed while trying to run reportWin()."})));
}

void AuctionRunner::OnReportBidWinComplete(
    const BidState* best_bid,
    const absl::optional<GURL>& bidder_report_url,
    const std::vector<std::string>& errors) {
  if (bidder_report_url && !IsUrlValid(*bidder_report_url)) {
    mojo::ReportBadMessage("Invalid bidder report URL");
    FailAuction();
    return;
  }

  bidder_report_url_ = bidder_report_url;
  errors_.insert(errors_.end(), errors.begin(), errors.end());
  ReportSuccess(best_bid);
}

void AuctionRunner::FailAuction() {
  DCHECK(callback_);
  ClosePipes();

  std::move(callback_).Run(this, absl::nullopt, absl::nullopt, absl::nullopt,
                           errors_);
}

void AuctionRunner::FailAuctionWithError(std::string error) {
  errors_.emplace_back(std::move(error));
  FailAuction();
}

void AuctionRunner::ReportSuccess(const BidState* state) {
  DCHECK(callback_);
  DCHECK(state->bid_result);
  ClosePipes();

  std::string ad_metadata;
  if (state->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s","metadata":%s})",
                           state->bid_result->render_url.spec().c_str(),
                           state->bid_ad->metadata.value().c_str());
  } else {
    ad_metadata = base::StringPrintf(
        R"({"render_url":"%s"})", state->bid_result->render_url.spec().c_str());
  }

  interest_group_manager_->RecordInterestGroupWin(
      state->bidder->group->owner, state->bidder->group->name, ad_metadata);

  std::move(callback_).Run(this, state->bid_result->render_url,
                           std::move(bidder_report_url_),
                           std::move(seller_report_url_), std::move(errors_));
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
