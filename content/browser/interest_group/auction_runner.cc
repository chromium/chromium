// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "net/base/escape.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// All URLs received from worklets must be valid HTTPS URLs. It's up to callers
// to call ReportBadMessage() on invalid URLs.
bool IsUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

// Finds InterestGroup::Ad in `ads` that matches `render_url`, if any. Returns
// nullptr if `render_url` is invalid.
const blink::InterestGroup::Ad* FindMatchingAd(
    const std::vector<blink::InterestGroup::Ad>& ads,
    const GURL& render_url) {
  // TODO(mmenke): Validate render URLs on load and make this a DCHECK just
  // before the return instead, since then `ads` will necessarily only contain
  // valid URLs at that point.
  if (!IsUrlValid(render_url))
    return nullptr;

  for (const auto& ad : ads) {
    if (ad.render_url == render_url) {
      return &ad;
    }
  }

  return nullptr;
}

// Validates that `bid` is valid and, if it is, returns the InterestGroupAd
// corresponding to the bid. Returns nullptr and calls ReportBadMessage() if
// not. If non-null, the returned pointer will point at the winning
// blink::mojom::InterestGroupAd within `bid`.
const blink::InterestGroup::Ad* ValidateBidAndGetAd(
    const auction_worklet::mojom::BidderWorkletBid& bid,
    const blink::InterestGroup& interest_group) {
  if (bid.bid <= 0 || std::isnan(bid.bid) || !std::isfinite(bid.bid)) {
    mojo::ReportBadMessage("Invalid bid value");
    return nullptr;
  }

  if (bid.bid_duration.is_negative()) {
    mojo::ReportBadMessage("Invalid bid duration");
    return nullptr;
  }

  const blink::InterestGroup::Ad* matching_ad =
      FindMatchingAd(*interest_group.ads, bid.render_url);
  if (!matching_ad) {
    mojo::ReportBadMessage("Bid render URL must be a valid ad URL");
    return nullptr;
  }

  // Validate `ad_component` URLs, if present.
  if (bid.ad_components) {
    // Only InterestGroups with ad components should return bids with ad
    // components.
    if (!interest_group.ad_components) {
      mojo::ReportBadMessage("Unexpected non-null ad component list");
      return nullptr;
    }

    if (bid.ad_components->size() > blink::kMaxAdAuctionAdComponents) {
      mojo::ReportBadMessage("Too many ad component URLs");
      return nullptr;
    }

    // Validate each ad component URL is valid and appears in the interest
    // group's `ad_components` field.
    for (const GURL& ad_component_url : *bid.ad_components) {
      if (!FindMatchingAd(*interest_group.ad_components, ad_component_url)) {
        mojo::ReportBadMessage(
            "Bid ad components URL must match a valid ad component URL");
        return nullptr;
      }
    }
  }

  return matching_ad;
}

}  // namespace

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    AuctionWorkletManager::Delegate* auction_worklet_manager_delegate,
    InterestGroupManager* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    std::vector<url::Origin> filtered_buyers,
    const url::Origin& frame_origin,
    RunAuctionCallback callback) {
  DCHECK(!filtered_buyers.empty());
  std::unique_ptr<AuctionRunner> instance(new AuctionRunner(
      auction_worklet_manager, auction_worklet_manager_delegate,
      interest_group_manager, std::move(auction_config), frame_origin,
      std::move(callback)));
  instance->ReadInterestGroups(std::move(filtered_buyers));
  return instance;
}

AuctionRunner::AuctionRunner(
    AuctionWorkletManager* auction_worklet_manager,
    AuctionWorkletManager::Delegate* auction_worklet_manager_delegate,
    InterestGroupManager* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    const url::Origin& frame_origin,
    RunAuctionCallback callback)
    : auction_worklet_manager_(auction_worklet_manager),
      auction_worklet_manager_delegate_(auction_worklet_manager_delegate),
      interest_group_manager_(interest_group_manager),
      auction_config_(std::move(auction_config)),
      frame_origin_(frame_origin),
      callback_(std::move(callback)) {}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::FailAuction(AuctionResult result,
                                const std::vector<std::string>& errors) {
  DCHECK(callback_);

  errors_.insert(errors_.end(), errors.begin(), errors.end());
  RecordResult(result);

  ClosePipes();

  std::move(callback_).Run(this, absl::nullopt, absl::nullopt, absl::nullopt,
                           absl::nullopt, errors_);
}

void AuctionRunner::ReadInterestGroups(
    std::vector<url::Origin> filtered_buyers) {
  num_pending_buyers_ = filtered_buyers.size();

  for (const url::Origin& buyer : filtered_buyers) {
    interest_group_manager_->GetInterestGroupsForOwner(
        buyer, base::BindOnce(&AuctionRunner::OnInterestGroupRead,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void AuctionRunner::OnInterestGroupRead(
    std::vector<StorageInterestGroup> interest_groups) {
  DCHECK_GT(num_pending_buyers_, 0u);
  --num_pending_buyers_;

  if (!interest_groups.empty()) {
    for (auto bidder = std::make_move_iterator(interest_groups.begin());
         bidder != std::make_move_iterator(interest_groups.end()); ++bidder) {
      // Ignore interest groups with no bidding script or no ads.
      if (!bidder->interest_group.bidding_url)
        continue;
      if (bidder->interest_group.ads->empty())
        continue;
      bid_states_.emplace_back(BidState());
      bid_states_.back().bidder = std::move(*bidder);
    }
    ++num_owners_with_interest_groups_;
  }

  // Wait for more buyers to be loaded, if there are still some pending.
  if (num_pending_buyers_ > 0)
    return;

  // Record histograms about the interest groups participating in the auction.
  UMA_HISTOGRAM_COUNTS_1000("Ads.InterestGroup.Auction.NumInterestGroups",
                            bid_states_.size());
  UMA_HISTOGRAM_COUNTS_100(
      "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
      num_owners_with_interest_groups_);

  // If no interest groups were found, end the auction without a winner.
  if (bid_states_.empty()) {
    FailAuction(AuctionResult::kNoInterestGroups);
    return;
  }

  num_bids_not_sent_to_seller_worklet_ = bid_states_.size();
  outstanding_bids_ = num_bids_not_sent_to_seller_worklet_;
  RequestSellerWorklet();
  RequestBidderWorklets();
}

void AuctionRunner::RequestSellerWorklet() {
  if (auction_worklet_manager_->RequestSellerWorklet(
          auction_config_->decision_logic_url,
          auction_config_->trusted_scoring_signals_url,
          base::BindOnce(&AuctionRunner::OnSellerWorkletReceived,
                         base::Unretained(this)),
          base::BindOnce(&AuctionRunner::OnSellerWorkletFatalError,
                         base::Unretained(this)),
          seller_worklet_handle_)) {
    OnSellerWorkletReceived();
  }
}

void AuctionRunner::OnSellerWorkletReceived() {
  DCHECK(!seller_worklet_received_);

  seller_worklet_received_ = true;
  for (auto& bid_state : bid_states_) {
    if (bid_state.state == BidState::State::kWaitingOnSellerWorkletLoad)
      ScoreBid(&bid_state);
  }
}

void AuctionRunner::RequestBidderWorklets() {
  // Auctions are only run when there are bidders participating. As-is, an
  // empty bidder vector here would result in synchronously calling back into
  // the creator, which isn't allowed.
  DCHECK(!bid_states_.empty());

  // Request processes for all bidder worklets.
  for (auto& bid_state : bid_states_) {
    DCHECK_EQ(bid_state.state,
              BidState::State::kLoadingWorkletsAndOnSellerProcess);

    bid_state.state = BidState::State::kWaitingForWorklet;
    if (RequestBidderWorklet(
            bid_state,
            base::BindOnce(&AuctionRunner::OnBidderWorkletReceived,
                           base::Unretained(this), &bid_state),
            base::BindOnce(&AuctionRunner::OnBidderWorkletGenerateBidFatalError,
                           base::Unretained(this), &bid_state))) {
      OnBidderWorkletReceived(&bid_state);
    }
  }
}

void AuctionRunner::OnSellerWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  AuctionResult result;
  switch (fatal_error_type) {
    case AuctionWorkletManager::FatalErrorType::kScriptLoadFailed:
      result = AuctionResult::kSellerWorkletLoadFailed;
      break;
    case AuctionWorkletManager::FatalErrorType::kWorkletCrash:
      result = AuctionResult::kSellerWorkletCrashed;
      break;
  }
  FailAuction(result, errors);
}

void AuctionRunner::OnBidderWorkletReceived(BidState* bid_state) {
  DCHECK_EQ(bid_state->state, BidState::State::kWaitingForWorklet);

  bid_state->state = BidState::State::kGeneratingBid;
  const blink::InterestGroup& interest_group = bid_state->bidder.interest_group;
  bid_state->worklet_handle->GetBidderWorklet()->GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParams::New(
          interest_group.name, interest_group.trusted_bidding_signals_keys,
          interest_group.user_bidding_signals, interest_group.ads,
          interest_group.ad_components),
      auction_config_->auction_ad_config_non_shared_params->auction_signals,
      PerBuyerSignals(bid_state), auction_config_->seller,
      bid_state->bidder.bidding_browser_signals.Clone(), auction_start_time_,
      base::BindOnce(&AuctionRunner::OnGenerateBidComplete,
                     weak_ptr_factory_.GetWeakPtr(), bid_state));

  // Invoke SendPendingSignalsRequests() asynchronously, if necessary. Do this
  // asynchronously so that all GenerateBid() calls that share a BidderWorklet
  // will have been invoked before the first SendPendingSignalsRequests() call.
  //
  // This relies on AuctionWorkletManager::Handle invoking all the callbacks
  // listening for creation of the same BidderWorklet synchronously.
  if (interest_group.trusted_bidding_signals_keys &&
      interest_group.trusted_bidding_signals_keys->size() > 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AuctionRunner::SendPendingSignalsRequestsForBidder,
                       weak_ptr_factory_.GetWeakPtr(), bid_state));
  }
}

void AuctionRunner::SendPendingSignalsRequestsForBidder(BidState* bid_state) {
  // Don't invoke callback if worklet was unloaded in the meantime.
  if (bid_state->worklet_handle)
    bid_state->worklet_handle->GetBidderWorklet()->SendPendingSignalsRequests();
}

void AuctionRunner::OnBidderWorkletGenerateBidFatalError(
    BidState* bid_state,
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  DCHECK_EQ(BidState::State::kGeneratingBid, bid_state->state);

  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    // Ignore default error message in case of crash. Instead, use a more
    // specific one.
    OnGenerateBidComplete(
        bid_state, auction_worklet::mojom::BidderWorkletBidPtr(),
        {base::StrCat({bid_state->bidder.interest_group.bidding_url->spec(),
                       " crashed while trying to run generateBid()."})});
    return;
  }

  // Otherwise, use error message from the worklet.
  OnGenerateBidComplete(bid_state,
                        auction_worklet::mojom::BidderWorkletBidPtr(), errors);
}

void AuctionRunner::OnGenerateBidComplete(
    BidState* state,
    auction_worklet::mojom::BidderWorkletBidPtr bid,
    const std::vector<std::string>& errors) {
  DCHECK(!state->bid_result);
  DCHECK_GT(num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(outstanding_bids_, 0);
  DCHECK_EQ(state->state, BidState::State::kGeneratingBid);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Release the worklet. If it wins the auction, it will it will be requested
  // again to invoke its ReportWin() method.
  state->worklet_handle.reset();

  // Ignore invalid bids.
  if (bid) {
    state->bid_ad = ValidateBidAndGetAd(*bid, state->bidder.interest_group);
    if (!state->bid_ad)
      bid.reset();
  }

  if (!bid) {
    state->state = BidState::State::kScoringComplete;
    --num_bids_not_sent_to_seller_worklet_;
    // If this is the only bid that yet to be sent to the seller worklet, and
    // the seller worklet has loaded, then tell the seller worklet to send any
    // pending scoring signals request to complete the auction more quickly.
    if (num_bids_not_sent_to_seller_worklet_ == 0)
      seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
    --outstanding_bids_;
    MaybeCompleteAuction();
    return;
  }

  state->bid_result = std::move(bid);
  state->state = BidState::State::kWaitingOnSellerWorkletLoad;
  if (seller_worklet_received_)
    ScoreBid(state);
}

void AuctionRunner::ScoreBid(BidState* state) {
  DCHECK_GT(num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(outstanding_bids_, 0);
  DCHECK_EQ(state->state, BidState::State::kWaitingOnSellerWorkletLoad);
  DCHECK(seller_worklet_received_);
  state->state = BidState::State::kSellerScoringBid;

  seller_worklet_handle_->GetSellerWorklet()->ScoreAd(
      state->bid_result->ad, state->bid_result->bid,
      auction_config_->auction_ad_config_non_shared_params.Clone(),
      state->bidder.interest_group.owner, state->bid_result->render_url,
      state->bid_result->ad_components ? *state->bid_result->ad_components
                                       : std::vector<GURL>(),
      state->bid_result->bid_duration.InMilliseconds(),
      base::BindOnce(&AuctionRunner::OnBidScored,
                     weak_ptr_factory_.GetWeakPtr(), state));

  // If this was the last bid that needed to be passed to ScoreAd(), tell the
  // SellerWorklet no more bids are coming, so it can send a request for any
  // needed scoring signals now, if needed.
  --num_bids_not_sent_to_seller_worklet_;
  if (num_bids_not_sent_to_seller_worklet_ == 0) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
  }
}

void AuctionRunner::OnBidScored(BidState* state,
                                double score,
                                const std::vector<std::string>& errors) {
  DCHECK_EQ(state->state, BidState::State::kSellerScoringBid);
  state->seller_score = score;
  --outstanding_bids_;
  state->state = BidState::State::kScoringComplete;
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // A score <= 0 means the seller rejected the bid.
  if (score > 0) {
    if (!top_bidder_ || score > top_bidder_->seller_score) {
      // If there's no previous top bidder, or the bidder has the highest score,
      // need to replace the previous top bidder.
      top_bidder_ = state;
      num_top_bidders_ = 1;
    } else if (score == top_bidder_->seller_score) {
      // If there's a tie, replace the top-bidder with 1-in-`num_top_bidders_`
      // chance. This is the select random value from a stream with fixed
      // storage problem.
      ++num_top_bidders_;
      if (1 == base::RandInt(1, num_top_bidders_))
        top_bidder_ = state;
    }
  }

  MaybeCompleteAuction();
}

absl::optional<std::string> AuctionRunner::PerBuyerSignals(
    const BidState* state) {
  const auto& per_buyer_signals =
      auction_config_->auction_ad_config_non_shared_params->per_buyer_signals;
  if (per_buyer_signals.has_value()) {
    auto it =
        per_buyer_signals.value().find(state->bidder.interest_group.owner);
    if (it != per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

void AuctionRunner::MaybeCompleteAuction() {
  if (!AllBidsScored())
    return;

  // Since all bids have been scored, they also should have all been sent to the
  // SellerWorklet by this point.
  DCHECK_EQ(0, num_bids_not_sent_to_seller_worklet_);

  // Record which interest groups bid.
  //
  // TODO(mmenke): Maybe this should be recorded at bid time, and the interest
  // group thrown away if it's not the top bid?
  bool some_bidder_bid = false;
  for (BidState& bid_state : bid_states_) {
    if (bid_state.bid_result) {
      some_bidder_bid = true;
      interest_group_manager_->RecordInterestGroupBid(
          bid_state.bidder.interest_group.owner,
          bid_state.bidder.interest_group.name);
    }
  }

  if (!top_bidder_) {
    FailAuction(some_bidder_bid ? AuctionResult::kAllBidsRejected
                                : AuctionResult::kNoBids);
    return;
  }

  // Will eventually send a report to the seller and clean up `this`.
  ReportSellerResult();
}

void AuctionRunner::ReportSellerResult() {
  DCHECK(top_bidder_);

  DCHECK(top_bidder_->bid_result);
  DCHECK_GT(top_bidder_->seller_score, 0);

  seller_worklet_handle_->GetSellerWorklet()->ReportResult(
      auction_config_->auction_ad_config_non_shared_params.Clone(),
      top_bidder_->bidder.interest_group.owner,
      top_bidder_->bid_result->render_url, top_bidder_->bid_result->bid,
      top_bidder_->seller_score,
      base::BindOnce(&AuctionRunner::OnReportSellerResultComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnReportSellerResultComplete(
    const absl::optional<std::string>& signals_for_winner,
    const absl::optional<GURL>& seller_report_url,
    const std::vector<std::string>& errors) {
  seller_report_url_ = seller_report_url;
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  absl::optional<GURL> opt_bidder_report_url;
  if (seller_report_url && !IsUrlValid(*seller_report_url)) {
    mojo::ReportBadMessage("Invalid seller report URL");
    FailAuction(AuctionResult::kBadMojoMessage);
    return;
  }

  LoadBidderWorkletToReportBidWin(signals_for_winner);
}

void AuctionRunner::LoadBidderWorkletToReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  DCHECK(top_bidder_->bid_result);

  // Worklet handle should have been destroyed once the bid was generated.
  DCHECK(!top_bidder_->worklet_handle);

  if (RequestBidderWorklet(
          *top_bidder_,
          base::BindOnce(&AuctionRunner::ReportBidWin, base::Unretained(this),
                         signals_for_winner),
          base::BindOnce(&AuctionRunner::OnWinningBidderWorkletFatalError,
                         base::Unretained(this)))) {
    ReportBidWin(signals_for_winner);
  }
}

void AuctionRunner::ReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  DCHECK(top_bidder_->bid_result);

  std::string signals_for_winner_arg;
  if (signals_for_winner) {
    signals_for_winner_arg = *signals_for_winner;
  } else {
    // `signals_for_winner_arg` is passed as JSON, so need to pass "null" when
    // it's not provided. Pass in "null" instead of making the API take an
    // optional to limit the information provided to the untrusted BidderWorklet
    // process that's not part of the FLEDGE API. Unlikely to matter, but best
    // to be safe.
    signals_for_winner_arg = "null";
  }

  top_bidder_->worklet_handle->GetBidderWorklet()->ReportWin(
      top_bidder_->bidder.interest_group.name,
      auction_config_->auction_ad_config_non_shared_params->auction_signals,
      PerBuyerSignals(top_bidder_), signals_for_winner_arg,
      top_bidder_->bid_result->render_url, top_bidder_->bid_result->bid,
      auction_config_->seller,
      base::BindOnce(&AuctionRunner::OnReportBidWinComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnReportBidWinComplete(
    const absl::optional<GURL>& bidder_report_url,
    const std::vector<std::string>& errors) {
  if (bidder_report_url && !IsUrlValid(*bidder_report_url)) {
    mojo::ReportBadMessage("Invalid bidder report URL");
    FailAuction(AuctionResult::kBadMojoMessage);
    return;
  }

  bidder_report_url_ = bidder_report_url;
  errors_.insert(errors_.end(), errors.begin(), errors.end());
  ReportSuccess();
}

void AuctionRunner::OnWinningBidderWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  // Crashes are considered fatal errors, while load errors currently are not.
  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    FailAuction(
        AuctionResult::kWinningBidderWorkletCrashed,
        // Ignore default error message in case of crash. Instead, use a more
        // specific one.
        {base::StrCat({top_bidder_->bidder.interest_group.bidding_url->spec(),
                       " crashed while trying to run reportWin()."})});
  } else {
    // An error while reloading the worklet to call ReportWin() does not
    // currently fail the auction.
    errors_.insert(errors_.end(), errors.begin(), errors.end());
    ReportSuccess();
  }
}

void AuctionRunner::ReportSuccess() {
  DCHECK(callback_);
  DCHECK(top_bidder_->bid_result);
  ClosePipes();

  RecordResult(AuctionResult::kSuccess);

  std::string ad_metadata;
  if (top_bidder_->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s","metadata":%s})",
                           top_bidder_->bid_result->render_url.spec().c_str(),
                           top_bidder_->bid_ad->metadata.value().c_str());
  } else {
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           top_bidder_->bid_result->render_url.spec().c_str());
  }

  interest_group_manager_->RecordInterestGroupWin(
      top_bidder_->bidder.interest_group.owner,
      top_bidder_->bidder.interest_group.name, ad_metadata);

  std::move(callback_).Run(this, top_bidder_->bid_result->render_url,
                           top_bidder_->bid_result->ad_components,
                           std::move(bidder_report_url_),
                           std::move(seller_report_url_), std::move(errors_));
}

void AuctionRunner::ClosePipes() {
  // This is needed in addition to closing worklet pipes in order to ignore
  // worklet creation callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  for (BidState& bid_state : bid_states_) {
    bid_state.worklet_handle.reset();
  }
  seller_worklet_handle_.reset();
}

void AuctionRunner::RecordResult(AuctionResult result) const {
  UMA_HISTOGRAM_ENUMERATION("Ads.InterestGroup.Auction.Result", result);

  // Only record time of full auctions and aborts.
  switch (result) {
    case AuctionResult::kAborted:
      UMA_HISTOGRAM_MEDIUM_TIMES("Ads.InterestGroup.Auction.AbortTime",
                                 base::Time::Now() - auction_start_time_);
      break;
    case AuctionResult::kNoBids:
    case AuctionResult::kAllBidsRejected:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
          base::Time::Now() - auction_start_time_);
      break;
    case AuctionResult::kSuccess:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
          base::Time::Now() - auction_start_time_);
      break;
    default:
      break;
  }
}

bool AuctionRunner::RequestBidderWorklet(
    BidState& bid_state,
    base::OnceClosure worklet_available_callback,
    AuctionWorkletManager::FatalErrorCallback fatal_error_callback) {
  DCHECK(!bid_state.worklet_handle);

  const blink::InterestGroup& interest_group = bid_state.bidder.interest_group;
  return auction_worklet_manager_->RequestBidderWorklet(
      interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url,
      std::move(worklet_available_callback), std::move(fatal_error_callback),
      bid_state.worklet_handle);
}

}  // namespace content
