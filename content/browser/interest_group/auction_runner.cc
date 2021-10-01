// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
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

  if (bid.bid_duration < base::TimeDelta()) {
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

void AuctionRunner::BidState::ClosePipes() {
  bidder_worklet_debug.reset();
  bidder_worklet.reset();
  process_handle.reset();
}

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
      std::move(browser_signals), frame_origin, std::move(callback)));
  instance->ReadInterestGroups(std::move(filtered_buyers));
  return instance;
}

AuctionRunner::AuctionRunner(
    Delegate* delegate,
    InterestGroupManager* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    auction_worklet::mojom::BrowserSignalsPtr browser_signals,
    const url::Origin& frame_origin,
    RunAuctionCallback callback)
    : delegate_(delegate),
      interest_group_manager_(interest_group_manager),
      auction_config_(std::move(auction_config)),
      browser_signals_(std::move(browser_signals)),
      frame_origin_(frame_origin),
      callback_(std::move(callback)) {}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::FailAuction(AuctionResult result,
                                absl::optional<std::string> error) {
  DCHECK(callback_);

  if (error)
    errors_.emplace_back(std::move(error).value());
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
    std::vector<BiddingInterestGroup> interest_groups) {
  DCHECK_GT(num_pending_buyers_, 0u);
  --num_pending_buyers_;

  if (!interest_groups.empty()) {
    for (auto bidder = std::make_move_iterator(interest_groups.begin());
         bidder != std::make_move_iterator(interest_groups.end()); ++bidder) {
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

  outstanding_bids_ = bid_states_.size();
  RequestSellerWorkletProcess();
}

void AuctionRunner::RequestSellerWorkletProcess() {
  seller_worklet_process_handle_ =
      std::make_unique<AuctionProcessManager::ProcessHandle>();

  // Request a seller worklet process. If one is received synchronously, start
  // loading the seller worklet and requesting bidder processes.
  if (interest_group_manager_->auction_process_manager().RequestWorkletService(
          AuctionProcessManager::WorkletType::kSeller, auction_config_->seller,
          seller_worklet_process_handle_.get(),
          base::BindOnce(&AuctionRunner::OnSellerWorkletProcessReceived,
                         weak_ptr_factory_.GetWeakPtr()))) {
    OnSellerWorkletProcessReceived();
  }
}

void AuctionRunner::OnSellerWorkletProcessReceived() {
  // Auctions are only run when there are bidders participating. As-is, an
  // empty bidder vector here would result in synchronously calling back into
  // the creator, which isn't allowed.
  DCHECK(!bid_states_.empty());

  // Start loading the seller worklet.
  const GURL& seller_url = auction_config_->decision_logic_url;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  seller_url_loader_factory_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      url_loader_factory.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&Delegate::GetFrameURLLoaderFactory,
                          base::Unretained(delegate_)),
      frame_origin_, true /* use_cors */, seller_url);
  bool should_pause_on_start = false;
  mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
      worklet_receiver = seller_worklet_.BindNewPipeAndPassReceiver();
  seller_worklet_debug_ = base::WrapUnique(new DebuggableAuctionWorklet(
      delegate_->GetFrame(), seller_url, seller_worklet_.get(),
      should_pause_on_start));
  seller_worklet_process_handle_->GetService()->LoadSellerWorklet(
      std::move(worklet_receiver), should_pause_on_start,
      std::move(url_loader_factory), seller_url,
      base::BindOnce(&AuctionRunner::OnSellerWorkletLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
  // Fail auction if the seller worklet pipe is disconnected.
  seller_worklet_.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuction, weak_ptr_factory_.GetWeakPtr(),
      AuctionResult::kSellerWorkletCrashed,
      base::StrCat({seller_url.spec(), " crashed."})));

  // Request processes for all bidder worklets.
  for (auto& bid_state : bid_states_) {
    DCHECK_EQ(bid_state.state,
              BidState::State::kLoadingWorkletsAndOnSellerProcess);

    bid_state.state = BidState::State::kWaitingForProcess;
    bid_state.process_handle =
        std::make_unique<AuctionProcessManager::ProcessHandle>();
    if (interest_group_manager_->auction_process_manager()
            .RequestWorkletService(
                AuctionProcessManager::WorkletType::kBidder,
                bid_state.bidder.group->group.owner,
                bid_state.process_handle.get(),
                base::BindOnce(&AuctionRunner::OnBidderWorkletProcessReceived,
                               weak_ptr_factory_.GetWeakPtr(), &bid_state))) {
      OnBidderWorkletProcessReceived(&bid_state);
    }
  }
}

void AuctionRunner::OnBidderWorkletProcessReceived(BidState* bid_state) {
  DCHECK_EQ(bid_state->state, BidState::State::kWaitingForProcess);

  bid_state->state = BidState::State::kGeneratingBid;
  auction_worklet::mojom::BiddingInterestGroup* bidder =
      bid_state->bidder.group.get();

  // Assemble list of URLs the bidder can request.

  // TODO(mmenke): This largely duplicates logic in the auction worklet
  // service. Avoid duplicating code.
  absl::optional<GURL> trusted_bidding_signals_full_url;
  if (bidder->group.trusted_bidding_signals_url &&
      bidder->group.trusted_bidding_signals_keys) {
    std::string query_params =
        "hostname=" + net::EscapeQueryParamValue(
                          browser_signals_->top_frame_origin.host(), true);
    query_params += "&keys=";
    bool first_key = true;
    for (const auto& key : *bidder->group.trusted_bidding_signals_keys) {
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
        bidder->group.trusted_bidding_signals_url->ReplaceComponents(
            replacements);
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  GURL bidding_url =
      bid_state->bidder.group->group.bidding_url.value_or(GURL());
  bid_state->url_loader_factory =
      std::make_unique<AuctionURLLoaderFactoryProxy>(
          url_loader_factory.InitWithNewPipeAndPassReceiver(),
          base::BindRepeating(&Delegate::GetTrustedURLLoaderFactory,
                              base::Unretained(delegate_)),
          frame_origin_, false /* use_cors */, bidding_url,
          trusted_bidding_signals_full_url);

  bid_state->state = BidState::State::kGeneratingBid;

  mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
      worklet_receiver = bid_state->bidder_worklet.BindNewPipeAndPassReceiver();
  bool should_pause_on_start = false;
  bid_state->bidder_worklet_debug =
      base::WrapUnique(new DebuggableAuctionWorklet(
          delegate_->GetFrame(), bidding_url, bid_state->bidder_worklet.get(),
          should_pause_on_start));

  bid_state->process_handle->GetService()->LoadBidderWorkletAndGenerateBid(
      std::move(worklet_receiver), should_pause_on_start,
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

void AuctionRunner::OnGenerateBidCrashed(BidState* state) {
  OnGenerateBidComplete(state, auction_worklet::mojom::BidderWorkletBidPtr(),
                        std::vector<std::string>{base::StrCat(
                            {state->bidder.group->group.bidding_url->spec(),
                             " crashed while trying to run generateBid()."})});
}

void AuctionRunner::OnGenerateBidComplete(
    BidState* state,
    auction_worklet::mojom::BidderWorkletBidPtr bid,
    const std::vector<std::string>& errors) {
  DCHECK(!state->bid_result);
  DCHECK_GT(outstanding_bids_, 0);
  DCHECK_EQ(state->state, BidState::State::kGeneratingBid);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Ignore invalid bids.
  if (bid) {
    state->bid_ad = ValidateBidAndGetAd(*bid, state->bidder.group->group);
    if (!state->bid_ad)
      bid.reset();
  }

  if (!bid) {
    // On failure, close the worklet pipes.
    state->ClosePipes();

    state->state = BidState::State::kScoringComplete;
    --outstanding_bids_;
    MaybeCompleteAuction();
    return;
  }

  // On success, clear the disconnect handler. After generating a bid, a crashed
  // bidder only matters if it's the winning bidder that crashed. That's checked
  // for at the end of the auction.
  state->bidder_worklet.set_disconnect_handler(base::OnceClosure());

  state->bid_result = std::move(bid);
  state->state = BidState::State::kWaitingOnSellerWorkletLoad;
  if (seller_loaded_)
    ScoreBid(state);
}

void AuctionRunner::OnSellerWorkletLoaded(
    bool load_result,
    const std::vector<std::string>& errors) {
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  if (!load_result) {
    // Failed to load the seller/auction script --- nothing useful can be
    // done, so abort, possibly cancelling other fetches, so we don't waste
    // time.
    FailAuction(AuctionResult::kSellerWorkletLoadFailed);
    return;
  }

  seller_loaded_ = true;
  // Start scoring any bids that were waiting on the seller worklet to load.
  for (BidState& state : bid_states_) {
    // Bids can be complete at this point (if no bid was offered, or on
    // error), but they can't be scoring a bid.
    DCHECK_NE(state.state, BidState::State::kSellerScoringBid);

    if (state.state == BidState::State::kWaitingOnSellerWorkletLoad)
      ScoreBid(&state);
  }
}

void AuctionRunner::ScoreBid(BidState* state) {
  DCHECK_EQ(state->state, BidState::State::kWaitingOnSellerWorkletLoad);
  state->state = BidState::State::kSellerScoringBid;
  seller_worklet_->ScoreAd(
      state->bid_result->ad, state->bid_result->bid, auction_config_.Clone(),
      browser_signals_->top_frame_origin, state->bidder.group->group.owner,
      AdRenderFingerprint(state),
      state->bid_result->bid_duration.InMilliseconds(),
      base::BindOnce(&AuctionRunner::OnBidScored,
                     weak_ptr_factory_.GetWeakPtr(), state));
}

void AuctionRunner::OnBidScored(BidState* state,
                                double score,
                                const std::vector<std::string>& errors) {
  DCHECK_EQ(state->state, BidState::State::kSellerScoringBid);
  state->seller_score = score;
  --outstanding_bids_;
  state->state = BidState::State::kScoringComplete;
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  if (score <= 0) {
    // If the worklet didn't bid, destroy the worklet.
    state->ClosePipes();
  } else {
    bool replace_top_bidder = false;
    if (!top_bidder_ || score > top_bidder_->seller_score) {
      // If there's no previous top bidder, or the bidder has the highest score,
      // need to replace the previous top bidder.
      replace_top_bidder = true;
      num_top_bidders_ = 1;
    } else if (score == top_bidder_->seller_score) {
      // If there's a tie, replace the top-bidder with 1-in-`num_top_bidders_`
      // chance. This is the select random value from a stream with fixed
      // storage problem.
      ++num_top_bidders_;
      if (1 == base::RandInt(1, num_top_bidders_))
        replace_top_bidder = true;
    }

    if (replace_top_bidder) {
      if (top_bidder_)
        top_bidder_->ClosePipes();
      top_bidder_ = state;
    } else {
      state->ClosePipes();
    }
  }

  MaybeCompleteAuction();
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
        state->bidder.group->group.owner);
    if (it != auction_config_->per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

void AuctionRunner::MaybeCompleteAuction() {
  if (!AllBidsScored())
    return;

  // Record which interest groups bid.
  //
  // TODO(mmenke): Maybe this should be recorded at bid time, and the interest
  // group thrown away if it's not the top bid?
  bool some_bidder_bid = false;
  for (BidState& bid_state : bid_states_) {
    if (bid_state.bid_result) {
      some_bidder_bid = true;
      interest_group_manager_->RecordInterestGroupBid(
          bid_state.bidder.group->group.owner,
          bid_state.bidder.group->group.name);
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
  DCHECK(top_bidder_->bidder_worklet);

  seller_worklet_->ReportResult(
      auction_config_.Clone(), browser_signals_->top_frame_origin,
      top_bidder_->bidder.group->group.owner,
      top_bidder_->bid_result->render_url, AdRenderFingerprint(top_bidder_),
      top_bidder_->bid_result->bid, top_bidder_->seller_score,
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

  ReportBidWin(signals_for_winner);
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

  // Fail the auction if the winning bidder process has crashed.
  if (!top_bidder_->bidder_worklet.is_connected()) {
    FailAuction(
        AuctionResult::kWinningBidderWorkletCrashed,
        base::StrCat({top_bidder_->bidder.group->group.bidding_url->spec(),
                      " crashed while idle."}));
    return;
  }

  top_bidder_->bidder_worklet->ReportWin(
      signals_for_winner_arg, top_bidder_->bid_result->render_url,
      AdRenderFingerprint(top_bidder_), top_bidder_->bid_result->bid,
      base::BindOnce(&AuctionRunner::OnReportBidWinComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  top_bidder_->bidder_worklet.set_disconnect_handler(base::BindOnce(
      &AuctionRunner::FailAuction, weak_ptr_factory_.GetWeakPtr(),
      AuctionResult::kWinningBidderWorkletCrashed,
      base::StrCat({top_bidder_->bidder.group->group.bidding_url->spec(),
                    " crashed while trying to run reportWin()."})));
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
      top_bidder_->bidder.group->group.owner,
      top_bidder_->bidder.group->group.name, ad_metadata);

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
    bid_state.ClosePipes();
  }
  seller_worklet_debug_.reset();
  seller_worklet_.reset();
  seller_worklet_process_handle_.reset();
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

}  // namespace content
