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
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
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

}  // namespace

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

AuctionRunner::Bid::Bid(std::string ad_metadata,
                        double bid,
                        GURL render_url,
                        absl::optional<std::vector<GURL>> ad_components,
                        base::TimeDelta bid_duration,
                        const blink::InterestGroup::Ad* bid_ad,
                        BidState* bid_state)
    : ad_metadata(std::move(ad_metadata)),
      bid(bid),
      render_url(std::move(render_url)),
      ad_components(std::move(ad_components)),
      bid_duration(bid_duration),
      interest_group(&bid_state->bidder.interest_group),
      bid_ad(bid_ad),
      bid_state(bid_state) {
  DCHECK_GT(bid, 0);
}

AuctionRunner::Bid::~Bid() = default;

AuctionRunner::ScoredBid::ScoredBid(double score,
                                    absl::optional<uint32_t> data_version,
                                    std::unique_ptr<Bid> bid)
    : score(score), data_version(data_version), bid(std::move(bid)) {
  DCHECK_GT(score, 0);
}

AuctionRunner::ScoredBid::~ScoredBid() = default;

AuctionRunner::Auction::Auction(
    blink::mojom::AuctionAdConfig* config,
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    base::Time auction_start_time)
    : auction_worklet_manager_(auction_worklet_manager),
      interest_group_manager_(interest_group_manager),
      config_(config),
      auction_start_time_(auction_start_time) {}

AuctionRunner::Auction::~Auction() = default;

void AuctionRunner::Auction::LoadInterestGroups(
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    LoadInterestGroupsCallback load_interest_groups_callback) {
  DCHECK(!load_interest_groups_callback_);
  DCHECK(!final_auction_result_);
  DCHECK_EQ(num_pending_buyers_, 0u);

  load_interest_groups_callback_ = std::move(load_interest_groups_callback);

  // If the seller can't participate in the auction, fail the auction.
  if (!is_interest_group_api_allowed_callback.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          config_->seller)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Auction::OnLoadInterestGroupsComplete,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  AuctionResult::kSellerRejected));
    return;
  }

  if (config_->auction_ad_config_non_shared_params->interest_group_buyers) {
    for (const auto& buyer :
         *config_->auction_ad_config_non_shared_params->interest_group_buyers) {
      if (!is_interest_group_api_allowed_callback.Run(
              ContentBrowserClient::InterestGroupApiOperation::kBuy, buyer)) {
        continue;
      }
      interest_group_manager_->GetInterestGroupsForOwner(
          buyer, base::BindOnce(&Auction::OnInterestGroupRead,
                                weak_ptr_factory_.GetWeakPtr()));
      ++num_pending_buyers_;
    }
  }

  // Fail if there are no pending loads.
  if (num_pending_buyers_ == 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Auction::OnLoadInterestGroupsComplete,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  AuctionResult::kNoInterestGroups));
  }
}

void AuctionRunner::Auction::ClosePipes() {
  // This is needed in addition to closing worklet pipes since the callbacks
  // passed to Mojo aren't currently cancellable.
  weak_ptr_factory_.InvalidateWeakPtrs();

  for (BidState& bid_state : bid_states_) {
    bid_state.worklet_handle.reset();
  }
  seller_worklet_handle_.reset();
}

AuctionRunner::InterestGroupSet
AuctionRunner::Auction::GetInterestGroupsThatBid() const {
  InterestGroupSet out;
  for (const BidState& bid_state : bid_states_) {
    if (bid_state.made_bid) {
      out.emplace(std::pair(bid_state.bidder.interest_group.owner,
                            bid_state.bidder.interest_group.name));
    }
  }

  return out;
}

void AuctionRunner::Auction::TakeDebugReportUrls(
    std::vector<GURL>& debug_win_report_urls,
    std::vector<GURL>& debug_loss_report_urls) {
  DCHECK(final_auction_result_);

  // Should only send loss report if the auction succeeded, or the seller
  // rejected all bids.
  if (*final_auction_result_ != AuctionResult::kSuccess &&
      *final_auction_result_ != AuctionResult::kAllBidsRejected) {
    return;
  }

  BidState* top_bidder = nullptr;
  if (top_bid_) {
    top_bidder = top_bid_->bid->bid_state;
    if (top_bidder->bidder_debug_win_report_url.has_value()) {
      debug_win_report_urls.emplace_back(
          std::move(top_bidder->bidder_debug_win_report_url).value());
    }
    if (top_bidder->seller_debug_win_report_url.has_value()) {
      debug_win_report_urls.emplace_back(
          std::move(top_bidder->seller_debug_win_report_url).value());
    }
  }

  for (BidState& bid_state : bid_states_) {
    if (&bid_state == top_bidder)
      continue;
    if (bid_state.bidder_debug_loss_report_url.has_value()) {
      debug_loss_report_urls.emplace_back(
          std::move(bid_state.bidder_debug_loss_report_url).value());
    }
    if (bid_state.seller_debug_loss_report_url.has_value()) {
      debug_loss_report_urls.emplace_back(
          std::move(bid_state.seller_debug_loss_report_url).value());
    }
  }
}

void AuctionRunner::Auction::OnInterestGroupRead(
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

  // If there are no bidders in this auction, fail the auction.
  if (bid_states_.empty()) {
    OnLoadInterestGroupsComplete(AuctionResult::kNoInterestGroups);
    return;
  }

  // There are bidders that can generate bids, so complete without a final
  // result.
  OnLoadInterestGroupsComplete(/*auction_result=*/AuctionResult::kSuccess);
}

void AuctionRunner::Auction::OnLoadInterestGroupsComplete(
    AuctionResult auction_result) {
  DCHECK(load_interest_groups_callback_);
  DCHECK(!final_auction_result_);

  // `final_auction_result_` should only be set to kSuccess when the entire
  // auction is complete.
  bool success = auction_result == AuctionResult::kSuccess;
  if (!success)
    final_auction_result_ = auction_result;
  std::move(load_interest_groups_callback_).Run(success);
}

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(
      new AuctionRunner(auction_worklet_manager, interest_group_manager,
                        std::move(auction_config), std::move(callback)));
  instance->StartAuction(is_interest_group_api_allowed_callback);
  return instance;
}

AuctionRunner::AuctionRunner(AuctionWorkletManager* auction_worklet_manager,
                             InterestGroupManagerImpl* interest_group_manager,
                             blink::mojom::AuctionAdConfigPtr auction_config,
                             RunAuctionCallback callback)
    : auction_worklet_manager_(auction_worklet_manager),
      interest_group_manager_(interest_group_manager),
      owned_auction_config_(std::move(auction_config)),
      callback_(std::move(callback)),
      auction_(owned_auction_config_.get(),
               auction_worklet_manager,
               interest_group_manager,
               /*auction_start_time=*/base::Time::Now()) {}

AuctionRunner::~AuctionRunner() = default;

std::unique_ptr<AuctionRunner::Bid> AuctionRunner::TryToCreateBid(
    auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
    BidState& bid_state,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url) {
  if (mojo_bid->bid <= 0 || std::isnan(mojo_bid->bid) ||
      !std::isfinite(mojo_bid->bid)) {
    mojo::ReportBadMessage("Invalid bid value");
    return nullptr;
  }

  if (mojo_bid->bid_duration.is_negative()) {
    mojo::ReportBadMessage("Invalid bid duration");
    return nullptr;
  }

  const blink::InterestGroup& interest_group = bid_state.bidder.interest_group;
  const blink::InterestGroup::Ad* matching_ad =
      FindMatchingAd(*interest_group.ads, mojo_bid->render_url);
  if (!matching_ad) {
    mojo::ReportBadMessage("Bid render URL must be a valid ad URL");
    return nullptr;
  }

  // Validate `ad_component` URLs, if present.
  if (mojo_bid->ad_components) {
    // Only InterestGroups with ad components should return bids with ad
    // components.
    if (!interest_group.ad_components) {
      mojo::ReportBadMessage("Unexpected non-null ad component list");
      return nullptr;
    }

    if (mojo_bid->ad_components->size() > blink::kMaxAdAuctionAdComponents) {
      mojo::ReportBadMessage("Too many ad component URLs");
      return nullptr;
    }

    // Validate each ad component URL is valid and appears in the interest
    // group's `ad_components` field.
    for (const GURL& ad_component_url : *mojo_bid->ad_components) {
      if (!FindMatchingAd(*interest_group.ad_components, ad_component_url)) {
        mojo::ReportBadMessage(
            "Bid ad components URL must match a valid ad component URL");
        return nullptr;
      }
    }
  }

  // Validate `debug_loss_report_url` and `debug_win_report_url`, if present.
  if (debug_loss_report_url.has_value() &&
      !IsUrlValid(debug_loss_report_url.value())) {
    mojo::ReportBadMessage("Invalid bidder debugging loss report URL");
    return nullptr;
  }
  if (debug_win_report_url.has_value() &&
      !IsUrlValid(debug_win_report_url.value())) {
    mojo::ReportBadMessage("Invalid bidder debugging win report URL");
    return nullptr;
  }

  return std::make_unique<Bid>(std::move(mojo_bid->ad), mojo_bid->bid,
                               std::move(mojo_bid->render_url),
                               std::move(mojo_bid->ad_components),
                               mojo_bid->bid_duration, matching_ad, &bid_state);
}

void AuctionRunner::FailAuction(AuctionResult result,
                                const std::vector<std::string>& errors) {
  DCHECK(callback_);

  auction_.errors_.insert(auction_.errors_.end(), errors.begin(), errors.end());
  RecordResult(result);

  ClosePipes();

  // Can have loss URLs if the auction failed because the seller rejected all
  // bids.
  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);
  // Shouldn't have any win report URLs if nothing won the auction.
  DCHECK(debug_win_report_urls.empty());

  std::move(callback_).Run(
      this, /*render_url=*/absl::nullopt,
      /*ad_component_urls=*/absl::nullopt,
      /*report_urls=*/{}, std::move(debug_loss_report_urls),
      std::move(debug_win_report_urls), std::move(auction_.errors_));
}

void AuctionRunner::StartAuction(
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback) {
  auction_.LoadInterestGroups(
      is_interest_group_api_allowed_callback,
      base::BindOnce(&AuctionRunner::OnInterestGroupsLoaded,
                     base::Unretained(this)));
}

void AuctionRunner::OnInterestGroupsLoaded(bool success) {
  if (!success) {
    FailAuction(*auction_.final_auction_result_);
    return;
  }

  auction_.num_bids_not_sent_to_seller_worklet_ = auction_.bid_states_.size();
  auction_.outstanding_bids_ = auction_.num_bids_not_sent_to_seller_worklet_;
  RequestSellerWorklet();
  RequestBidderWorklets();
}

void AuctionRunner::RequestSellerWorklet() {
  if (auction_worklet_manager_->RequestSellerWorklet(
          auction_.config_->decision_logic_url,
          auction_.config_->trusted_scoring_signals_url,
          base::BindOnce(&AuctionRunner::OnSellerWorkletReceived,
                         base::Unretained(this)),
          base::BindOnce(&AuctionRunner::OnSellerWorkletFatalError,
                         base::Unretained(this)),
          auction_.seller_worklet_handle_)) {
    OnSellerWorkletReceived();
  }
}

void AuctionRunner::OnSellerWorkletReceived() {
  DCHECK(!auction_.seller_worklet_received_);

  auction_.seller_worklet_received_ = true;
  for (auto& unscored_bid : auction_.unscored_bids_) {
    ScoreBid(std::move(unscored_bid));
  }
  auction_.unscored_bids_.clear();
}

void AuctionRunner::RequestBidderWorklets() {
  // Auctions are only run when there are bidders participating. As-is, an
  // empty bidder vector here would result in synchronously calling back into
  // the creator, which isn't allowed.
  DCHECK(!auction_.bid_states_.empty());

  // Request processes for all bidder worklets.
  for (auto& bid_state : auction_.bid_states_) {
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
  const blink::InterestGroup& interest_group = bid_state->bidder.interest_group;
  bid_state->worklet_handle->GetBidderWorklet()->GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParams::New(
          interest_group.name, interest_group.trusted_bidding_signals_keys,
          interest_group.user_bidding_signals, interest_group.ads,
          interest_group.ad_components),
      auction_.config_->auction_ad_config_non_shared_params->auction_signals,
      PerBuyerSignals(bid_state), auction_.config_->seller,
      bid_state->bidder.bidding_browser_signals.Clone(),
      auction_.auction_start_time_,
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
  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    // Ignore default error message in case of crash. Instead, use a more
    // specific one.
    OnGenerateBidComplete(
        bid_state, auction_worklet::mojom::BidderWorkletBidPtr(),
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        {base::StrCat({bid_state->bidder.interest_group.bidding_url->spec(),
                       " crashed while trying to run generateBid()."})});
    return;
  }

  // Otherwise, use error message from the worklet.
  OnGenerateBidComplete(bid_state,

                        auction_worklet::mojom::BidderWorkletBidPtr(),
                        /*debug_loss_report_url=*/absl::nullopt,
                        /*debug_win_report_url=*/absl::nullopt, errors);
}

void AuctionRunner::OnGenerateBidComplete(
    BidState* state,
    auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    const std::vector<std::string>& errors) {
  DCHECK(!state->made_bid);
  DCHECK_GT(auction_.num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(auction_.outstanding_bids_, 0);

  auction_.errors_.insert(auction_.errors_.end(), errors.begin(), errors.end());

  // Release the worklet. If it wins the auction, it will be requested again to
  // invoke its ReportWin() method.
  state->worklet_handle.reset();

  // Ignore invalid bids.
  std::unique_ptr<Bid> bid;
  std::string ad_metadata;
  // `mojo_bid` is null if the worklet doesn't bid, or if the bidder worklet
  // fails to load / crashes.
  if (mojo_bid) {
    bid = TryToCreateBid(std::move(mojo_bid), *state, debug_loss_report_url,
                         debug_win_report_url);
    if (bid)
      state->bidder_debug_loss_report_url = std::move(debug_loss_report_url);
  } else {
    // Bidders who do not bid are allowed to get loss report.
    state->bidder_debug_loss_report_url = std::move(debug_loss_report_url);
  }

  if (!bid) {
    --auction_.num_bids_not_sent_to_seller_worklet_;
    // If this is the only bid that yet to be sent to the seller worklet, and
    // the seller worklet has loaded, then tell the seller worklet to send any
    // pending scoring signals request to complete the auction more quickly.
    if (auction_.num_bids_not_sent_to_seller_worklet_ == 0) {
      auction_.seller_worklet_handle_->GetSellerWorklet()
          ->SendPendingSignalsRequests();
    }
    --auction_.outstanding_bids_;
    MaybeCompleteAuction();
    return;
  }

  state->bidder_debug_win_report_url = std::move(debug_win_report_url);
  state->made_bid = true;
  if (!auction_.seller_worklet_received_) {
    auction_.unscored_bids_.emplace_back(std::move(bid));
  } else {
    ScoreBid(std::move(bid));
  }
}

void AuctionRunner::ScoreBid(std::unique_ptr<Bid> bid) {
  DCHECK(bid);
  DCHECK_GT(auction_.num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(auction_.outstanding_bids_, 0);
  DCHECK(bid->bid_state->made_bid);
  DCHECK(auction_.seller_worklet_received_);

  Bid* bid_raw = bid.get();
  auction_.seller_worklet_handle_->GetSellerWorklet()->ScoreAd(
      bid_raw->ad_metadata, bid_raw->bid,
      auction_.config_->auction_ad_config_non_shared_params.Clone(),
      bid_raw->interest_group->owner, bid_raw->render_url,
      bid_raw->ad_components ? *bid_raw->ad_components : std::vector<GURL>(),
      bid_raw->bid_duration.InMilliseconds(),
      base::BindOnce(&AuctionRunner::OnBidScored,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bid)));

  // If this was the last bid that needed to be passed to ScoreAd(), tell the
  // SellerWorklet no more bids are coming, so it can send a request for any
  // needed scoring signals now, if needed.
  --auction_.num_bids_not_sent_to_seller_worklet_;
  if (auction_.num_bids_not_sent_to_seller_worklet_ == 0) {
    auction_.seller_worklet_handle_->GetSellerWorklet()
        ->SendPendingSignalsRequests();
  }
}

void AuctionRunner::OnBidScored(
    std::unique_ptr<Bid> bid,
    double score,
    uint32_t data_version,
    bool has_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    const std::vector<std::string>& errors) {
  --auction_.outstanding_bids_;

  // If `debug_loss_report_url` or `debug_win_report_url` is not a valid HTTPS
  // URL, the auction should fail because the worklet is compromised.
  if (debug_loss_report_url.has_value() &&
      !IsUrlValid(debug_loss_report_url.value())) {
    mojo::ReportBadMessage("Invalid seller debugging loss report URL");
    FailAuction(AuctionResult::kBadMojoMessage);
    return;
  }
  if (debug_win_report_url.has_value() &&
      !IsUrlValid(debug_win_report_url.value())) {
    mojo::ReportBadMessage("Invalid seller debugging win report URL");
    FailAuction(AuctionResult::kBadMojoMessage);
    return;
  }
  auction_.errors_.insert(auction_.errors_.end(), errors.begin(), errors.end());

  bid->bid_state->seller_debug_loss_report_url =
      std::move(debug_loss_report_url);
  bid->bid_state->seller_debug_win_report_url = std::move(debug_win_report_url);

  bool is_top_bid = false;

  // A score <= 0 means the seller rejected the bid.
  if (score > 0) {
    if (!auction_.top_bid_ || score > auction_.top_bid_->score) {
      // If there's no previous top bidder, or the bidder has the highest score,
      // need to replace the previous top bidder.
      auction_.num_top_bids_ = 1;
      is_top_bid = true;
    } else if (score == auction_.top_bid_->score) {
      // If there's a tie, replace the top-bidder with 1-in-`num_top_bidders_`
      // chance. This is the select random value from a stream with fixed
      // storage problem.
      ++auction_.num_top_bids_;
      if (1 == base::RandInt(1, auction_.num_top_bids_))
        is_top_bid = true;
    }
  }

  if (is_top_bid) {
    auction_.top_bid_ = std::make_unique<ScoredBid>(
        score, has_data_version ? data_version : absl::optional<uint32_t>(),
        std::move(bid));
  }

  MaybeCompleteAuction();
}

absl::optional<std::string> AuctionRunner::PerBuyerSignals(
    const BidState* state) {
  const auto& per_buyer_signals =
      auction_.config_->auction_ad_config_non_shared_params->per_buyer_signals;
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
  DCHECK_EQ(0, auction_.num_bids_not_sent_to_seller_worklet_);

  // Record which interest groups bid.
  //
  // TODO(mmenke): Maybe this should be recorded at bid time, and the interest
  // group thrown away if it's not the top bid?
  InterestGroupSet interest_groups_that_bid =
      auction_.GetInterestGroupsThatBid();
  for (const auto& interest_group : interest_groups_that_bid) {
    interest_group_manager_->RecordInterestGroupBid(
        /*owner=*/interest_group.first,
        /*name=*/interest_group.second);
  }

  if (!auction_.top_bid_) {
    // If there are no bids, fail with either kAllBidsRejected or kNoBids,
    // depending on whether any bidders bid.
    for (BidState& bid_state : auction_.bid_states_) {
      if (bid_state.made_bid) {
        FailAuction(AuctionResult::kAllBidsRejected);
        return;
      }
    }
    FailAuction(AuctionResult::kNoBids);
    return;
  }

  // Will eventually send a report to the seller and clean up `this`.
  ReportSellerResult();
}

void AuctionRunner::ReportSellerResult() {
  DCHECK(auction_.top_bid_);

  auction_.seller_worklet_handle_->GetSellerWorklet()->ReportResult(
      auction_.config_->auction_ad_config_non_shared_params.Clone(),
      auction_.top_bid_->bid->interest_group->owner,
      auction_.top_bid_->bid->render_url, auction_.top_bid_->bid->bid,
      auction_.top_bid_->score, auction_.top_bid_->data_version.value_or(0),
      auction_.top_bid_->data_version.has_value(),
      base::BindOnce(&AuctionRunner::OnReportSellerResultComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnReportSellerResultComplete(
    const absl::optional<std::string>& signals_for_winner,
    const absl::optional<GURL>& seller_report_url,
    const std::vector<std::string>& errors) {
  // There should be no other report URLs at this point.
  DCHECK(auction_.report_urls_.empty());

  if (seller_report_url) {
    if (!IsUrlValid(*seller_report_url)) {
      mojo::ReportBadMessage("Invalid seller report URL");
      FailAuction(AuctionResult::kBadMojoMessage);
      return;
    }

    auction_.report_urls_.push_back(*seller_report_url);
  }

  auction_.errors_.insert(auction_.errors_.end(), errors.begin(), errors.end());
  LoadBidderWorkletToReportBidWin(signals_for_winner);
}

void AuctionRunner::LoadBidderWorkletToReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  DCHECK(auction_.top_bid_);

  // Worklet handle should have been destroyed once the bid was generated.
  DCHECK(!auction_.top_bid_->bid->bid_state->worklet_handle);

  if (RequestBidderWorklet(
          *auction_.top_bid_->bid->bid_state,
          base::BindOnce(&AuctionRunner::ReportBidWin, base::Unretained(this),
                         signals_for_winner),
          base::BindOnce(&AuctionRunner::OnWinningBidderWorkletFatalError,
                         base::Unretained(this)))) {
    ReportBidWin(signals_for_winner);
  }
}

void AuctionRunner::ReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  DCHECK(auction_.top_bid_);

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

  auction_.top_bid_->bid->bid_state->worklet_handle->GetBidderWorklet()
      ->ReportWin(auction_.top_bid_->bid->interest_group->name,
                  auction_.config_->auction_ad_config_non_shared_params
                      ->auction_signals,
                  PerBuyerSignals(auction_.top_bid_->bid->bid_state),
                  signals_for_winner_arg, auction_.top_bid_->bid->render_url,
                  auction_.top_bid_->bid->bid, auction_.config_->seller,
                  base::BindOnce(&AuctionRunner::OnReportBidWinComplete,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::OnReportBidWinComplete(
    const absl::optional<GURL>& bidder_report_url,
    const std::vector<std::string>& errors) {
  // There should be at most one other report URL at this point.
  DCHECK_LE(auction_.report_urls_.size(), 1u);

  if (bidder_report_url) {
    if (!IsUrlValid(*bidder_report_url)) {
      mojo::ReportBadMessage("Invalid bidder report URL");
      FailAuction(AuctionResult::kBadMojoMessage);
      return;
    }

    auction_.report_urls_.push_back(*bidder_report_url);
  }

  auction_.errors_.insert(auction_.errors_.end(), errors.begin(), errors.end());
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
        {base::StrCat(
            {auction_.top_bid_->bid->interest_group->bidding_url->spec(),
             " crashed while trying to run reportWin()."})});
  } else {
    // An error while reloading the worklet to call ReportWin() does not
    // currently fail the auction.
    auction_.errors_.insert(auction_.errors_.end(), errors.begin(),
                            errors.end());
    ReportSuccess();
  }
}

void AuctionRunner::ReportSuccess() {
  DCHECK(callback_);
  DCHECK(auction_.top_bid_);
  DCHECK_LE(auction_.report_urls_.size(), 2u);

  ClosePipes();

  RecordResult(AuctionResult::kSuccess);

  std::string ad_metadata;
  if (auction_.top_bid_->bid->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    ad_metadata = base::StringPrintf(
        R"({"render_url":"%s","metadata":%s})",
        auction_.top_bid_->bid->render_url.spec().c_str(),
        auction_.top_bid_->bid->bid_ad->metadata.value().c_str());
  } else {
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           auction_.top_bid_->bid->render_url.spec().c_str());
  }

  interest_group_manager_->RecordInterestGroupWin(
      auction_.top_bid_->bid->interest_group->owner,
      auction_.top_bid_->bid->interest_group->name, ad_metadata);

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);

  std::move(callback_).Run(
      this, auction_.top_bid_->bid->render_url,
      auction_.top_bid_->bid->ad_components, std::move(auction_.report_urls_),
      std::move(debug_loss_report_urls), std::move(debug_win_report_urls),
      std::move(auction_.errors_));
}

void AuctionRunner::ClosePipes() {
  // This is needed in addition to closing worklet pipes since the callbacks
  // passed to Mojo aren't currently cancellable.
  weak_ptr_factory_.InvalidateWeakPtrs();

  auction_.ClosePipes();
}

void AuctionRunner::RecordResult(AuctionResult result) {
  // TODO(mmenke): Remove this, once Auction always sets this on completion.
  auction_.final_auction_result_ = result;

  UMA_HISTOGRAM_ENUMERATION("Ads.InterestGroup.Auction.Result", result);

  // Only record time of full auctions and aborts.
  switch (result) {
    case AuctionResult::kAborted:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Ads.InterestGroup.Auction.AbortTime",
          base::Time::Now() - auction_.auction_start_time_);
      break;
    case AuctionResult::kNoBids:
    case AuctionResult::kAllBidsRejected:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
          base::Time::Now() - auction_.auction_start_time_);
      break;
    case AuctionResult::kSuccess:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
          base::Time::Now() - auction_.auction_start_time_);
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
