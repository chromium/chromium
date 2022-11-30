// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_auction.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_priority_util.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::ReportingDestination;
constexpr base::TimeDelta kMaxTimeout = base::Milliseconds(500);
constexpr base::TimeDelta kKAnonymityExpiration = base::Days(7);

// For group freshness metrics.
constexpr base::TimeDelta kGroupFreshnessMin = base::Minutes(1);
constexpr base::TimeDelta kGroupFreshnessMax = base::Days(30);
constexpr int kGroupFreshnessBuckets = 100;

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

// Checks that `bid` is a valid bid value for an auction.
bool IsValidBid(double bid) {
  return !std::isnan(bid) && std::isfinite(bid) && bid > 0;
}

struct BidStatesDescByPriority {
  bool operator()(const std::unique_ptr<InterestGroupAuction::BidState>& a,
                  const std::unique_ptr<InterestGroupAuction::BidState>& b) {
    return a->calculated_priority > b->calculated_priority;
  }
  bool operator()(const std::unique_ptr<InterestGroupAuction::BidState>& a,
                  double b_priority) {
    return a->calculated_priority > b_priority;
  }
  bool operator()(double a_priority,
                  const std::unique_ptr<InterestGroupAuction::BidState>& b) {
    return a_priority > b->calculated_priority;
  }
};

struct BidStatesDescByPriorityAndGroupByJoinOrigin {
  bool operator()(const std::unique_ptr<InterestGroupAuction::BidState>& a,
                  const std::unique_ptr<InterestGroupAuction::BidState>& b) {
    return std::tie(a->calculated_priority, a->bidder.joining_origin,
                    a->bidder.interest_group.execution_mode) >
           std::tie(b->calculated_priority, b->bidder.joining_origin,
                    b->bidder.interest_group.execution_mode);
  }
};

}  // namespace

InterestGroupAuction::BidState::BidState() = default;

InterestGroupAuction::BidState::~BidState() {
  if (trace_id.has_value())
    EndTracing();
}

InterestGroupAuction::BidState::BidState(BidState&&) = default;

InterestGroupAuction::BidState& InterestGroupAuction::BidState::operator=(
    BidState&&) = default;

void InterestGroupAuction::BidState::BeginTracing() {
  DCHECK(!trace_id.has_value());

  trace_id = base::trace_event::GetNextGlobalTraceId();

  const blink::InterestGroup& interest_group = bidder.interest_group;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("fledge", "bid", *trace_id, "bidding_url",
                                    interest_group.bidding_url,
                                    "interest_group_name", interest_group.name);
}

void InterestGroupAuction::BidState::EndTracing() {
  DCHECK(trace_id.has_value());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bid", *trace_id);
  trace_id = absl::nullopt;
}

InterestGroupAuction::Bid::Bid(
    std::string ad_metadata,
    double bid,
    GURL render_url,
    std::vector<GURL> ad_components,
    base::TimeDelta bid_duration,
    absl::optional<uint32_t> bidding_signals_data_version,
    const blink::InterestGroup::Ad* bid_ad,
    BidState* bid_state,
    InterestGroupAuction* auction)
    : ad_metadata(std::move(ad_metadata)),
      bid(bid),
      render_url(std::move(render_url)),
      ad_components(std::move(ad_components)),
      bid_duration(bid_duration),
      bidding_signals_data_version(bidding_signals_data_version),
      interest_group(&bid_state->bidder.interest_group),
      bid_ad(bid_ad),
      bid_state(bid_state),
      auction(auction) {
  DCHECK(IsValidBid(bid));
}

InterestGroupAuction::Bid::Bid(Bid&) = default;

InterestGroupAuction::Bid::~Bid() = default;

InterestGroupAuction::ScoredBid::ScoredBid(
    double score,
    absl::optional<uint32_t> scoring_signals_data_version,
    std::unique_ptr<Bid> bid,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params)
    : score(score),
      scoring_signals_data_version(scoring_signals_data_version),
      bid(std::move(bid)),
      component_auction_modified_bid_params(
          std::move(component_auction_modified_bid_params)) {
  DCHECK_GT(score, 0);
}

InterestGroupAuction::ScoredBid::~ScoredBid() = default;

// Every interest group owner participating in an auctions gets its own
// BuyerHelper. The class is responsible for handing buyer-side calls during
// the bidding/scoring phase.
//
// In particular, it handles:
// * Sorting interest groups that share a bidder by priority.
// * Deciding which interest groups get to bid.
// * Creating BidderWorklets.
// * Calling BidderWorklet::GenerateBid().
// * Tracking how many interest groups the buyer owns that still need to
// bid.
class InterestGroupAuction::BuyerHelper
    : public auction_worklet::mojom::GenerateBidClient {
 public:
  // `auction` is expected to own the BuyerHelper, and therefore outlive it.
  BuyerHelper(InterestGroupAuction* auction,
              std::vector<StorageInterestGroup> interest_groups)
      : auction_(auction), owner_(interest_groups[0].interest_group.owner) {
    DCHECK(!interest_groups.empty());

    // Move interest groups to `bid_states_` and update priorities using
    // `priority_vector`, if present. Delete groups where the calculation
    // results in a priority < 0.
    for (auto& bidder : interest_groups) {
      double priority = bidder.interest_group.priority;

      if (bidder.interest_group.priority_vector &&
          !bidder.interest_group.priority_vector->empty()) {
        priority = CalculateInterestGroupPriority(
            *auction_->config_, bidder, auction_->auction_start_time_,
            *bidder.interest_group.priority_vector);
        // Only filter interest groups with priority < 0 if the negative
        // priority is the result of a `priority_vector` multiplication.
        //
        // TODO(mmenke): If we can make this the standard behavior for the
        // `priority` field as well, the API would be more consistent.
        if (priority < 0)
          continue;
      }

      if (bidder.interest_group.enable_bidding_signals_prioritization)
        enable_bidding_signals_prioritization_ = true;

      auto state = std::make_unique<BidState>();
      state->bidder = std::move(bidder);
      state->calculated_priority = priority;
      bid_states_.emplace_back(std::move(state));
    }

    size_limit_ = auction_->config_->non_shared_params.all_buyers_group_limit;
    const auto limit_iter =
        auction_->config_->non_shared_params.per_buyer_group_limits.find(
            owner_);
    if (limit_iter !=
        auction_->config_->non_shared_params.per_buyer_group_limits.cend()) {
      size_limit_ = static_cast<size_t>(limit_iter->second);
    }
    size_limit_ = std::min(bid_states_.size(), size_limit_);
    if (size_limit_ == 0) {
      bid_states_.clear();
      return;
    }

    if (!enable_bidding_signals_prioritization_) {
      ApplySizeLimitAndSort();
    } else {
      // When not applying the size limit yet, still sort by priority, since
      // worklets preserve the order they see requests in. This allows higher
      // priority interest groups will get to bid first, and also groups
      // interest groups by the origin they joined to potentially improve
      // Javscript context reuse.
      SortByPriorityAndGroupByJoinOrigin();
    }
  }

  ~BuyerHelper() override = default;

  // Requests bidder worklets and starts generating bids. May generate no bids,
  // 1 bid, or multiple bids. Invokes owning InterestGroupAuction's
  // ScoreBidIfReady() for each bid generated, and OnBidderDone() once all bids
  // have been generated. OnBidderDone() is always invoked asynchronously.
  void StartGeneratingBids() {
    DCHECK(!bid_states_.empty());
    DCHECK_EQ(0, num_outstanding_bids_);
    num_outstanding_bids_ = bid_states_.size();
    num_outstanding_bidding_signals_received_calls_ = num_outstanding_bids_;

    // Request processes for all bidder worklets.
    for (auto& bid_state : bid_states_) {
      if (auction_->RequestBidderWorklet(
              *bid_state,
              base::BindOnce(&BuyerHelper::OnBidderWorkletReceived,
                             base::Unretained(this), bid_state.get()),
              base::BindOnce(&BuyerHelper::OnBidderWorkletGenerateBidFatalError,
                             base::Unretained(this), bid_state.get()))) {
        OnBidderWorkletReceived(bid_state.get());
      }
    }
  }

  // auction_worklet::mojom::GenerateBidClient implementation:

  void OnBiddingSignalsReceived(
      const base::flat_map<std::string, double>& priority_vector,
      base::OnceClosure resume_generate_bid_callback) override {
    BidState* state = generate_bid_client_receiver_set_.current_context();
    absl::optional<double> new_priority;
    if (!priority_vector.empty()) {
      const auto& interest_group = state->bidder.interest_group;
      new_priority = CalculateInterestGroupPriority(
          *auction_->config_, state->bidder, auction_->auction_start_time_,
          priority_vector,
          (interest_group.priority_vector &&
           !interest_group.priority_vector->empty())
              ? state->calculated_priority
              : absl::optional<double>());
    }
    OnBiddingSignalsReceivedInternal(state, new_priority,
                                     std::move(resume_generate_bid_callback));
  }

  void OnGenerateBidComplete(
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      auction_worklet::mojom::BidderWorkletBidPtr alternate_bid,
      uint32_t bidding_signals_data_version,
      bool has_bidding_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      double set_priority,
      bool has_set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& errors) override {
    OnGenerateBidCompleteInternal(
        generate_bid_client_receiver_set_.current_context(),
        std::move(mojo_bid), bidding_signals_data_version,
        has_bidding_signals_data_version, debug_loss_report_url,
        debug_win_report_url, set_priority, has_set_priority,
        std::move(update_priority_signals_overrides), std::move(pa_requests),
        errors);
  }

  // Closes all Mojo pipes and release all weak pointers.
  void ClosePipes() {
    // This is needed in addition to closing worklet pipes since the callbacks
    // passed to Mojo pipes this class doesn't own aren't cancellable.
    weak_ptr_factory_.InvalidateWeakPtrs();

    for (auto& bid_state : bid_states_) {
      CloseBidStatePipes(*bid_state);
    }
    // No need to clear `generate_bid_client_receiver_set_`, since
    // CloseBidStatePipes() should take care of that.
    DCHECK(generate_bid_client_receiver_set_.empty());
  }

  // Returns true if this buyer has any interest groups that will potentially
  // bid in an auction -- that is, not all interest groups have been filtered
  // out.
  bool has_potential_bidder() const { return !bid_states_.empty(); }

  size_t num_potential_bidders() const { return bid_states_.size(); }

  const url::Origin& owner() const { return owner_; }

  void GetInterestGroupsThatBid(
      blink::InterestGroupSet& interest_groups) const {
    for (const auto& bid_state : bid_states_) {
      if (bid_state->made_bid) {
        interest_groups.emplace(bid_state->bidder.interest_group.owner,
                                bid_state->bidder.interest_group.name);
      }
    }
  }

  // Adds debug reporting URLs to `debug_win_report_urls` and
  // `debug_loss_report_urls`, if there are any, filling in report URL template
  // parameters as needed.
  //
  // `winner` is the BidState associated with the winning bid, if there is one.
  // If it's not a BidState managed by `this`, it has no effect.
  //
  // `signals` are the PostAuctionSignals from the auction `this` was a part of.
  //
  // `top_level_signals` are the PostAuctionSignals of the top-level auction, if
  // this is a component auction, and nullopt otherwise.
  void TakeDebugReportUrls(
      const BidState* winner,
      const PostAuctionSignals& signals,
      const absl::optional<PostAuctionSignals>& top_level_signals,
      std::vector<GURL>& debug_win_report_urls,
      std::vector<GURL>& debug_loss_report_urls) {
    for (auto& bid_state : bid_states_) {
      if (bid_state.get() == winner) {
        if (winner->bidder_debug_win_report_url.has_value()) {
          debug_win_report_urls.emplace_back(FillPostAuctionSignals(
              std::move(winner->bidder_debug_win_report_url).value(), signals));
        }
        if (winner->seller_debug_win_report_url.has_value()) {
          debug_win_report_urls.emplace_back(FillPostAuctionSignals(
              std::move(winner->seller_debug_win_report_url).value(), signals,
              top_level_signals));
        }
        // `top_level_signals` is passed as parameter `signals` for top-level
        // seller.
        if (winner->top_level_seller_debug_win_report_url.has_value()) {
          debug_win_report_urls.emplace_back(FillPostAuctionSignals(
              std::move(winner->top_level_seller_debug_win_report_url).value(),
              top_level_signals.value()));
        }
        continue;
      }
      if (bid_state->bidder_debug_loss_report_url.has_value()) {
        // Losing and rejected bidders should not get highest_scoring_other_bid
        // and made_highest_scoring_other_bid signals.
        debug_loss_report_urls.emplace_back(FillPostAuctionSignals(
            std::move(bid_state->bidder_debug_loss_report_url).value(),
            PostAuctionSignals(signals.winning_bid, signals.made_winning_bid,
                               0.0, false),
            /*top_level_signals=*/absl::nullopt, bid_state->reject_reason));
      }
      // TODO(qingxinwu): Add reject reason to seller debug loss report as well.
      if (bid_state->seller_debug_loss_report_url.has_value()) {
        debug_loss_report_urls.emplace_back(FillPostAuctionSignals(
            std::move(bid_state->seller_debug_loss_report_url).value(), signals,
            top_level_signals));
      }
      // `top_level_signals` is passed as parameter `signals` for top-level
      // seller.
      if (bid_state->top_level_seller_debug_loss_report_url.has_value()) {
        debug_loss_report_urls.emplace_back(FillPostAuctionSignals(
            std::move(bid_state->top_level_seller_debug_loss_report_url)
                .value(),
            top_level_signals.value()));
      }
    }
  }

 private:
  // Sorts by descending priority, also grouping entries within each priority
  // band to permit context reuse if the executionMode allows it.
  void SortByPriorityAndGroupByJoinOrigin() {
    std::sort(bid_states_.begin(), bid_states_.end(),
              BidStatesDescByPriorityAndGroupByJoinOrigin());
  }

  // Applies `size_limit_`, removing the lowest priority interest groups first,
  // and then sorts the remaining interest groups.
  void ApplySizeLimitAndSort() {
    SortByPriorityAndGroupByJoinOrigin();

    // Randomize order of interest groups with lowest allowed priority. This
    // effectively performs a random sample among interest groups with the
    // same priority.
    double min_priority = bid_states_[size_limit_ - 1]->calculated_priority;
    auto rand_begin = std::lower_bound(bid_states_.begin(), bid_states_.end(),
                                       min_priority, BidStatesDescByPriority());
    auto rand_end = std::upper_bound(rand_begin, bid_states_.end(),
                                     min_priority, BidStatesDescByPriority());
    base::RandomShuffle(rand_begin, rand_end);
    for (size_t i = size_limit_; i < bid_states_.size(); ++i) {
      // Need to close pipes explicitly, as the state's GenerateBidClientPipe is
      // owned by `generate_bid_client_receiver_set_`, deleting the bid isn't
      // sufficient.
      CloseBidStatePipes(*bid_states_[i]);
    }
    bid_states_.resize(size_limit_);

    // Restore the origin grouping within lowest priority band among the
    // subset that was kept after shuffling.
    std::sort(rand_begin, bid_states_.end(),
              BidStatesDescByPriorityAndGroupByJoinOrigin());
  }

  // Called when the `bid_state` BidderWorklet crashes or fails to load.
  // Invokes OnGenerateBidCompleteInternal() for the worklet with a failure.
  void OnBidderWorkletGenerateBidFatalError(
      BidState* bid_state,
      AuctionWorkletManager::FatalErrorType fatal_error_type,
      const std::vector<std::string>& errors) {
    // Add error(s) directly to error list.
    if (fatal_error_type ==
        AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
      // Ignore default error message in case of crash. Instead, use a more
      // specific one.
      auction_->errors_.push_back(
          base::StrCat({bid_state->bidder.interest_group.bidding_url->spec(),
                        " crashed while trying to run generateBid()."}));
    } else {
      auction_->errors_.insert(auction_->errors_.end(), errors.begin(),
                               errors.end());
    }

    // If waiting on bidding signals, the bidder needs to be removed in the same
    // way as if it had a new negative priority value, so reuse that logic. The
    // bidder needs to be removed, and the remaining bidders potentially need to
    // have the size limit applied and have their generate bid calls resumed, if
    // they were waiting on this bidder. Therefore, can't just call
    // OnGenerateBidCompleteInternal().
    if (!bid_state->bidding_signals_received) {
      OnBiddingSignalsReceivedInternal(bid_state,
                                       /*new_priority=*/-1,
                                       base::OnceClosure());
      return;
    }

    // Otherwise call OnGenerateBidCompleteInternal() directly to complete the
    // bid. This will also result in closing pipes. If
    // `enable_bidding_signals_prioritization_` is true, the closed pipe will be
    // noticed, and it will be removed before applying the priority filter.
    OnGenerateBidCompleteInternal(bid_state,
                                  auction_worklet::mojom::BidderWorkletBidPtr(),
                                  /*bidding_signals_data_version=*/0,
                                  /*has_bidding_signals_data_version=*/false,
                                  /*debug_loss_report_url=*/absl::nullopt,
                                  /*debug_win_report_url=*/absl::nullopt,
                                  /*set_priority=*/0,
                                  /*has_set_priority=*/false,
                                  /*update_priority_signals_overrides=*/{},
                                  /*pa_requests=*/{},
                                  /*errors=*/{});
  }

  base::flat_map<GURL, bool> ComputeKAnon(
      const StorageInterestGroup& storage_interest_group,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode) {
    if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kNone) {
      return base::flat_map<GURL, bool>();
    }

    // k-anon cache is always checked against the same time, to avoid weird
    // behavior of validity changing in the middle of the auction.
    base::Time start_time = auction_->auction_start_time_;

    std::vector<std::pair<GURL, bool>> kanon_entries;
    for (const auto& ad_kanon : storage_interest_group.ads_kanon) {
      bool is_kanon =
          ad_kanon.is_k_anonymous &&
          (ad_kanon.last_updated + kKAnonymityExpiration < start_time);
      if (is_kanon)
        kanon_entries.emplace_back(ad_kanon.key, true);
    }
    return base::flat_map<GURL, bool>(std::move(kanon_entries));
  }

  // Invoked whenever the AuctionWorkletManager has provided a BidderWorket
  // for the bidder identified by `bid_state`. Starts generating a bid.
  void OnBidderWorkletReceived(BidState* bid_state) {
    const blink::InterestGroup& interest_group =
        bid_state->bidder.interest_group;

    bid_state->BeginTracing();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "bidder_worklet_generate_bid",
                                      *bid_state->trace_id);

    mojo::PendingAssociatedRemote<auction_worklet::mojom::GenerateBidClient>
        pending_remote;
    bid_state->generate_bid_client_receiver_id =
        generate_bid_client_receiver_set_.Add(
            this, pending_remote.InitWithNewEndpointAndPassReceiver(),
            bid_state);
    auction_worklet::mojom::KAnonymityBidMode kanon_mode =
        auction_worklet::mojom::KAnonymityBidMode::kNone;
    bid_state->worklet_handle->GetBidderWorklet()->GenerateBid(
        auction_worklet::mojom::BidderWorkletNonSharedParams::New(
            interest_group.name,
            interest_group.enable_bidding_signals_prioritization,
            interest_group.priority_vector, interest_group.execution_mode,
            interest_group.daily_update_url,
            interest_group.trusted_bidding_signals_keys,
            interest_group.user_bidding_signals, interest_group.ads,
            interest_group.ad_components,
            ComputeKAnon(bid_state->bidder, kanon_mode)),
        kanon_mode, bid_state->bidder.joining_origin,
        auction_->config_->non_shared_params.auction_signals,
        GetPerBuyerSignals(*auction_->config_,
                           bid_state->bidder.interest_group.owner),
        auction_->PerBuyerTimeout(bid_state), auction_->config_->seller,
        auction_->parent_ ? auction_->parent_->config_->seller
                          : absl::optional<url::Origin>(),
        bid_state->bidder.bidding_browser_signals.Clone(),
        auction_->auction_start_time_, *bid_state->trace_id,
        std::move(pending_remote));

    // Invoke SendPendingSignalsRequests() asynchronously, if necessary. Do this
    // asynchronously so that all GenerateBid() calls that share a BidderWorklet
    // will have been invoked before the first SendPendingSignalsRequests()
    // call.
    //
    // This relies on AuctionWorkletManager::Handle invoking all the callbacks
    // listening for creation of the same BidderWorklet synchronously.
    if (interest_group.trusted_bidding_signals_url) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&BuyerHelper::SendPendingSignalsRequestsForBidder,
                         weak_ptr_factory_.GetWeakPtr(), bid_state));
    }
  }

  // Invoked when OnBiddingSignalsReceived() has been called for `state`, or
  // with a negative priority when the worklet process has an error and is
  // waiting on the OnBiddingSignalsReceived() invocation.
  void OnBiddingSignalsReceivedInternal(
      BidState* state,
      absl::optional<double> new_priority,
      base::OnceClosure resume_generate_bid_callback) {
    DCHECK(!state->bidding_signals_received);
    DCHECK(state->generate_bid_client_receiver_id);
    DCHECK_GT(num_outstanding_bids_, 0);
    DCHECK_GT(num_outstanding_bidding_signals_received_calls_, 0);
    // `resume_generate_bid_callback` must be non-null except when invoked with
    // a negative `net_priority` on worklet error.
    DCHECK(resume_generate_bid_callback || *new_priority < 0);

    state->bidding_signals_received = true;
    --num_outstanding_bidding_signals_received_calls_;

    // If `new_priority` has a value and is negative, need to record the bidder
    // as no longer participating in the auction and cancel bid generation.
    if (new_priority.has_value() && *new_priority < 0) {
      // Record if there are other bidders, as if there are not, the next call
      // may delete `this`.
      bool other_bidders = (num_outstanding_bids_ > 1);

      // If the result of applying the filter is negative, complete the bid
      // with OnGenerateBidCompleteInternal(), which will close the relevant
      // pipes and abort bid generation.
      OnGenerateBidCompleteInternal(
          state, auction_worklet::mojom::BidderWorkletBidPtr(),
          /*bidding_signals_data_version=*/0,
          /*has_bidding_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          /*set_priority=*/0,
          /*has_set_priority=*/false,
          /*update_priority_signals_overrides=*/{},
          /*pa_requests=*/{},
          /*errors=*/{});
      // If this was the last bidder, and it was filtered out, there's nothing
      // else to do, and `this` may have already been deleted.
      if (!other_bidders)
        return;

      // If bidding_signals_prioritization is not enabled, there's also
      // nothing else to do - no other bidders were blocked on the bidder's
      // OnBiddingSignalsReceived() call.
      if (!enable_bidding_signals_prioritization_)
        return;
    } else {
      if (new_priority.has_value())
        state->calculated_priority = *new_priority;
      // Otherwise, invoke the callback to proceed to generate a bid, if don't
      // need to prioritize / filter based on number of interest groups.
      if (!enable_bidding_signals_prioritization_) {
        std::move(resume_generate_bid_callback).Run();
        return;
      }

      state->resume_generate_bid_callback =
          std::move(resume_generate_bid_callback);
    }

    // Check if there are any outstanding OnBiddingSignalsReceived() calls. If
    // so, need to sort interest groups by priority resume pending generate bid
    // calls.
    DCHECK(enable_bidding_signals_prioritization_);
    if (num_outstanding_bidding_signals_received_calls_ > 0)
      return;

    // Remove Bid states that were filtered out due to having negative new
    // priorities, as ApplySizeLimitAndSort() assumes all bidders are still
    // potentially capable of generating bids. Do these all at once to avoid
    // repeatedly searching for bid stats that had negative priority vector
    // multiplication results, each time a priority vector is received.
    for (size_t i = 0; i < bid_states_.size();) {
      // Removing a bid is guaranteed to destroy the worklet handle, though not
      // necessarily the `resume_generate_bid_callback` (in particular,
      // OnBidderWorkletGenerateBidFatalError() calls OnGenerateBidInternal() if
      // a worklet with a `resume_generate_bid_callback` already set crashes,
      // but does not clear `resume_generate_bid_callback`, since doing so
      // directly without closing the pipe first will DCHECK).
      if (!bid_states_[i]->worklet_handle) {
        // The GenerateBidClient pipe should also have been closed.
        DCHECK(!bid_states_[i]->generate_bid_client_receiver_id);
        // std::swap() instead of std::move() because self-move isn't guaranteed
        // to work.
        std::swap(bid_states_[i], bid_states_.back());
        bid_states_.pop_back();
        continue;
      }
      DCHECK(bid_states_[i]->resume_generate_bid_callback);
      ++i;
    }

    // The above loop should have deleted any bid states not accounted for in
    // `num_outstanding_bids_`.
    DCHECK_EQ(static_cast<size_t>(num_outstanding_bids_), bid_states_.size());

    ApplySizeLimitAndSort();

    // Update `num_outstanding_bids_` to reflect the remaining number of pending
    // bids, after applying the size limit.
    num_outstanding_bids_ = bid_states_.size();

    // Let all generate bid calls proceed.
    for (auto& pending_state : bid_states_) {
      std::move(pending_state->resume_generate_bid_callback).Run();
    }
  }

  // Called once a bid has been generated, or has failed to be generated.
  // Releases the BidderWorklet handle and instructs the SellerWorklet to
  // start scoring the bid, if there is one.
  void OnGenerateBidCompleteInternal(
      BidState* state,
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      uint32_t bidding_signals_data_version,
      bool has_bidding_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url,
      double set_priority,
      bool has_set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      const std::vector<std::string>& errors) {
    DCHECK(!state->made_bid);
    DCHECK_GT(num_outstanding_bids_, 0);

    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidder_worklet_generate_bid",
                                    *state->trace_id);

    absl::optional<uint32_t> maybe_bidding_signals_data_version;
    if (has_bidding_signals_data_version)
      maybe_bidding_signals_data_version = bidding_signals_data_version;

    if (has_set_priority) {
      auction_->interest_group_manager_->SetInterestGroupPriority(
          blink::InterestGroupKey(state->bidder.interest_group.owner,
                                  state->bidder.interest_group.name),
          set_priority);
    }

    if (!update_priority_signals_overrides.empty()) {
      // Reject infinite values. The worklet code should prevent this, but the
      // process may be compromised. This is largely preventing the owner from
      // messing up its own prioritization function, but there could be issues
      // around serializing infinite values to persist to disk as well.
      //
      // Note that the data received here has no effect on the result of the
      // auction, so just reject the data and continue with the auction to keep
      // the code simple.
      if (base::ranges::any_of(
              update_priority_signals_overrides, [](const auto& pair) {
                return pair.second && !std::isfinite(pair.second->value);
              })) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Invalid priority signals overrides");
      } else {
        auction_->interest_group_manager_->UpdateInterestGroupPriorityOverrides(
            blink::InterestGroupKey(state->bidder.interest_group.owner,
                                    state->bidder.interest_group.name),
            std::move(update_priority_signals_overrides));
      }
    }

    // The mojom API declaration should ensure none of these are null.
    DCHECK(base::ranges::none_of(
        pa_requests,
        [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
               request_ptr) { return request_ptr.is_null(); }));
    if (!pa_requests.empty()) {
      PrivateAggregationRequests& pa_requests_for_bidder =
          auction_->private_aggregation_requests_[state->bidder.interest_group
                                                      .owner];
      pa_requests_for_bidder.insert(pa_requests_for_bidder.end(),
                                    std::move_iterator(pa_requests.begin()),
                                    std::move_iterator(pa_requests.end()));
    }

    auction_->errors_.insert(auction_->errors_.end(), errors.begin(),
                             errors.end());

    // Ignore invalid bids.
    std::unique_ptr<Bid> bid;
    std::string ad_metadata;
    // `mojo_bid` is null if the worklet doesn't bid, or if the bidder worklet
    // fails to load / crashes.
    if (mojo_bid) {
      bid = TryToCreateBid(std::move(mojo_bid), *state,
                           maybe_bidding_signals_data_version,
                           debug_loss_report_url, debug_win_report_url);
      if (bid)
        state->bidder_debug_loss_report_url = debug_loss_report_url;
    } else {
      // Bidders who do not bid are allowed to get loss report.
      state->bidder_debug_loss_report_url = debug_loss_report_url;
    }

    // Release the worklet. If it wins the auction, it will be requested again
    // to invoke its ReportWin() method.
    CloseBidStatePipes(*state);

    if (!bid) {
      state->EndTracing();
    } else {
      state->bidder_debug_win_report_url = debug_win_report_url;
      state->made_bid = true;
      auction_->ScoreBidIfReady(std::move(bid));
    }

    --num_outstanding_bids_;
    if (num_outstanding_bids_ == 0) {
      DCHECK_EQ(num_outstanding_bidding_signals_received_calls_, 0);
      auction_->OnBidSourceDone();
    }
  }

  // Calls SendPendingSignalsRequests() for the BidderWorklet of `bid_state`,
  // if it hasn't been destroyed. This is done asynchronously, so that
  // BidStates that share a BidderWorklet all call GenerateBid() before this
  // is invoked for all of them.
  //
  // This does result in invoking SendPendingSignalsRequests() multiple times
  // for BidStates that share BidderWorklets, though that should be fairly low
  // overhead.
  void SendPendingSignalsRequestsForBidder(BidState* bid_state) {
    // Don't invoke callback if worklet was unloaded in the meantime.
    if (bid_state->worklet_handle)
      bid_state->worklet_handle->GetBidderWorklet()
          ->SendPendingSignalsRequests();
  }

  // Validates that `mojo_bid` is valid and, if it is, creates a Bid
  // corresponding to it, consuming it. Returns nullptr and calls
  // ReportBadMessage() if it's not valid. Does not mutate `bid_state`, but
  // the returned Bid has a non-const pointer to it.
  std::unique_ptr<InterestGroupAuction::Bid> TryToCreateBid(
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      BidState& bid_state,
      const absl::optional<uint32_t>& bidding_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url) {
    if (!IsValidBid(mojo_bid->bid)) {
      generate_bid_client_receiver_set_.ReportBadMessage("Invalid bid value");
      return nullptr;
    }

    if (mojo_bid->bid_duration.is_negative()) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bid duration");
      return nullptr;
    }

    const blink::InterestGroup& interest_group =
        bid_state.bidder.interest_group;
    const blink::InterestGroup::Ad* matching_ad =
        FindMatchingAd(*interest_group.ads, mojo_bid->render_url);
    if (!matching_ad) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Bid render URL must be a valid ad URL");
      return nullptr;
    }

    // Validate `ad_component` URLs, if present.
    std::vector<GURL> ad_components;
    if (mojo_bid->ad_components) {
      // Only InterestGroups with ad components should return bids with ad
      // components.
      if (!interest_group.ad_components) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Unexpected non-null ad component list");
        return nullptr;
      }

      if (mojo_bid->ad_components->size() > blink::kMaxAdAuctionAdComponents) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Too many ad component URLs");
        return nullptr;
      }

      // Validate each ad component URL is valid and appears in the interest
      // group's `ad_components` field.
      for (const GURL& ad_component_url : *mojo_bid->ad_components) {
        if (!FindMatchingAd(*interest_group.ad_components, ad_component_url)) {
          generate_bid_client_receiver_set_.ReportBadMessage(
              "Bid ad components URL must match a valid ad component URL");
          return nullptr;
        }
      }
      ad_components = *std::move(mojo_bid->ad_components);
    }

    // Validate `debug_loss_report_url` and `debug_win_report_url`, if present.
    if (debug_loss_report_url.has_value() &&
        !IsUrlValid(debug_loss_report_url.value())) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bidder debugging loss report URL");
      return nullptr;
    }
    if (debug_win_report_url.has_value() &&
        !IsUrlValid(debug_win_report_url.value())) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bidder debugging win report URL");
      return nullptr;
    }

    return std::make_unique<Bid>(
        std::move(mojo_bid->ad), mojo_bid->bid, std::move(mojo_bid->render_url),
        std::move(ad_components), mojo_bid->bid_duration,
        bidding_signals_data_version, matching_ad, &bid_state, auction_);
  }

  // Close all Mojo pipes associated with `state`.
  void CloseBidStatePipes(BidState& state) {
    state.worklet_handle.reset();
    if (state.generate_bid_client_receiver_id) {
      generate_bid_client_receiver_set_.Remove(
          *state.generate_bid_client_receiver_id);
      state.generate_bid_client_receiver_id.reset();
    }
  }

  size_t size_limit_;

  const raw_ptr<InterestGroupAuction> auction_;

  const url::Origin owner_;

  // State of loaded interest groups owned by `owner_`. Use unique_ptrs so that
  // pointers aren't invalidated by sorting / deleting BidStates.
  std::vector<std::unique_ptr<BidState>> bid_states_;

  // Per-BidState receivers. These can never be null. Uses unique_ptrs so that
  // existing pointers aren't invalidated by sorting / deleting BidStates.
  mojo::AssociatedReceiverSet<auction_worklet::mojom::GenerateBidClient,
                              BidState*>
      generate_bid_client_receiver_set_;

  int num_outstanding_bidding_signals_received_calls_ = 0;
  int num_outstanding_bids_ = 0;

  // True if any interest group owned by `owner_` participating in this auction
  // has `use_biddings_signals_prioritization` set to true. When this is true,
  // all GenerateBid() calls will be deferred until OnBiddingSignalsReceived()
  // has been invoked for all bidders (or they've failed to generate bids due to
  // errors).
  //
  // TODO(mmenke): Could only set this to true if the number of bidders exceeds
  // the per-buyer limit as well, and only the `priority_vector` as a filter for
  // buyers with `use_biddings_signals_prioritization` set to true, as a small
  // performance optimization.
  bool enable_bidding_signals_prioritization_ = false;

  base::WeakPtrFactory<BuyerHelper> weak_ptr_factory_{this};
};

InterestGroupAuction::InterestGroupAuction(
    const blink::AuctionConfig* config,
    const InterestGroupAuction* parent,
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    base::Time auction_start_time)
    : trace_id_(base::trace_event::GetNextGlobalTraceId()),
      auction_worklet_manager_(auction_worklet_manager),
      interest_group_manager_(interest_group_manager),
      config_(config),
      parent_(parent),
      auction_start_time_(auction_start_time) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("fledge", "auction", trace_id_,
                                    "decision_logic_url",
                                    config_->decision_logic_url);

  for (const auto& component_auction_config :
       config->non_shared_params.component_auctions) {
    // Nested component auctions are not supported.
    DCHECK(!parent_);
    component_auctions_.emplace_back(std::make_unique<InterestGroupAuction>(
        &component_auction_config, /*parent=*/this, auction_worklet_manager,
        interest_group_manager, auction_start_time));
  }
}

InterestGroupAuction::~InterestGroupAuction() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "auction", trace_id_);

  if (!final_auction_result_)
    final_auction_result_ = AuctionResult::kAborted;

  // TODO(mmenke): Record histograms for component auctions.
  if (!parent_) {
    UMA_HISTOGRAM_ENUMERATION("Ads.InterestGroup.Auction.Result",
                              *final_auction_result_);

    // Only record time of full auctions and aborts.
    switch (*final_auction_result_) {
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
}

void InterestGroupAuction::StartLoadInterestGroupsPhase(
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    AuctionPhaseCompletionCallback load_interest_groups_phase_callback) {
  DCHECK(is_interest_group_api_allowed_callback);
  DCHECK(load_interest_groups_phase_callback);
  DCHECK(buyer_helpers_.empty());
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK_EQ(num_pending_loads_, 0u);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "load_groups_phase", trace_id_);

  load_interest_groups_phase_callback_ =
      std::move(load_interest_groups_phase_callback);

  // If the seller can't participate in the auction, fail the auction.
  if (!is_interest_group_api_allowed_callback.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          config_->seller)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
            weak_ptr_factory_.GetWeakPtr(), AuctionResult::kSellerRejected));
    return;
  }

  for (auto component_auction = component_auctions_.begin();
       component_auction != component_auctions_.end(); ++component_auction) {
    (*component_auction)
        ->StartLoadInterestGroupsPhase(
            is_interest_group_api_allowed_callback,
            base::BindOnce(&InterestGroupAuction::OnComponentInterestGroupsRead,
                           weak_ptr_factory_.GetWeakPtr(), component_auction));
    ++num_pending_loads_;
  }

  if (config_->non_shared_params.interest_group_buyers) {
    for (const auto& buyer :
         *config_->non_shared_params.interest_group_buyers) {
      if (!is_interest_group_api_allowed_callback.Run(
              ContentBrowserClient::InterestGroupApiOperation::kBuy, buyer)) {
        continue;
      }
      interest_group_manager_->GetInterestGroupsForOwner(
          buyer, base::BindOnce(&InterestGroupAuction::OnInterestGroupRead,
                                weak_ptr_factory_.GetWeakPtr()));
      ++num_pending_loads_;
    }
  }

  // Fail if there are no pending loads.
  if (num_pending_loads_ == 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
            weak_ptr_factory_.GetWeakPtr(), AuctionResult::kNoInterestGroups));
  }
}

void InterestGroupAuction::StartBiddingAndScoringPhase(
    base::OnceClosure on_seller_receiver_callback,
    AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback) {
  DCHECK(bidding_and_scoring_phase_callback);
  DCHECK(!buyer_helpers_.empty() || !component_auctions_.empty());
  DCHECK(!on_seller_receiver_callback_);
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK(!top_bid_);
  DCHECK_EQ(pending_component_seller_worklet_requests_, 0u);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "bidding_and_scoring_phase",
                                    trace_id_);

  on_seller_receiver_callback_ = std::move(on_seller_receiver_callback);
  bidding_and_scoring_phase_callback_ =
      std::move(bidding_and_scoring_phase_callback);

  outstanding_bid_sources_ = buyer_helpers_.size() + component_auctions_.size();

  // Need to start loading worklets before any bids can be generated or scored.

  if (component_auctions_.empty()) {
    // If there are no component auctions, request the seller worklet.
    // Otherwise, the seller worklet will be requested once all component
    // auctions have received their own seller worklets.
    RequestSellerWorklet();
  } else {
    // Since component auctions may invoke OnComponentSellerWorkletReceived()
    // synchronously, it's important to set this to the total number of
    // component auctions before invoking StartBiddingAndScoringPhase() on any
    // component auction.
    pending_component_seller_worklet_requests_ = component_auctions_.size();
    for (auto& component_auction : component_auctions_) {
      component_auction->StartBiddingAndScoringPhase(
          base::BindOnce(
              &InterestGroupAuction::OnComponentSellerWorkletReceived,
              base::Unretained(this)),
          base::BindOnce(&InterestGroupAuction::OnComponentAuctionComplete,
                         base::Unretained(this), component_auction.get()));
    }
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    buyer_helper->StartGeneratingBids();
  }
}

void InterestGroupAuction::StartReportingPhase(
    absl::optional<std::string> top_seller_signals,
    AuctionPhaseCompletionCallback reporting_phase_callback) {
  DCHECK(reporting_phase_callback);
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK(top_bid_);
  // This should only be called on top-level auctions.
  DCHECK(!parent_);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "reporting_phase", trace_id_);

  InterestGroupAuctionReporter::WinningBidInfo winning_bid_info;
  winning_bid_info.storage_interest_group = &top_bid_->bid->bid_state->bidder;
  winning_bid_info.render_url = top_bid_->bid->render_url;
  winning_bid_info.ad_components = top_bid_->bid->ad_components;
  // Need the bid from the bidder itself. If the bid was from a component
  // auction, then `top_bid_->bid` will be the bid from the component auction,
  // which the component seller worklet may have modified, and thus the wrong
  // bid. As a result, have to get the top bid from the component auction in
  // that case. `top_bid_->bid->auction->top_bid()` is the same as `top_bid_` if
  // the bid was from the top-level auction, and the original top bid from the
  // component auction, otherwise, so will always be the bid returned by the
  // winning bidder's generateBid() method.
  winning_bid_info.bid = top_bid_->bid->auction->top_bid()->bid->bid;
  winning_bid_info.bid_duration = top_bid_->bid->bid_duration;
  winning_bid_info.bidding_signals_data_version =
      top_bid_->bid->bidding_signals_data_version;

  InterestGroupAuctionReporter::SellerWinningBidInfo
      top_level_seller_winning_bid_info;
  top_level_seller_winning_bid_info.auction_config = config_;
  top_level_seller_winning_bid_info.bid = top_bid_->bid->bid;
  top_level_seller_winning_bid_info.score = top_bid_->score;
  top_level_seller_winning_bid_info.highest_scoring_other_bid =
      highest_scoring_other_bid_;
  top_level_seller_winning_bid_info.highest_scoring_other_bid_owner =
      highest_scoring_other_bid_owner_;
  top_level_seller_winning_bid_info.scoring_signals_data_version =
      top_bid_->scoring_signals_data_version;
  top_level_seller_winning_bid_info.trace_id = trace_id_;

  // Populate the SellerWinningBidInfo for the component auction that the
  // winning bid came from, if any. This largely duplicates the above block.
  //
  // TODO(mmenke): Share code with the above block. This currently isn't
  // possible because InterestGroupAuctionReporter depends on
  // InterestGroupAuction, so it can return an auction completion status, so no
  // InterestGroupAuction methods can take or return an
  // InterestGroupAuctionReporter::SellerWinningBidInfo. Once that dependency is
  // removed, it should be possible to make a helper method to construct both
  // SellerWinningBidInfos.
  absl::optional<InterestGroupAuctionReporter::SellerWinningBidInfo>
      component_seller_winning_bid_info;
  if (top_bid_->bid->auction != this) {
    const InterestGroupAuction* component_auction = top_bid_->bid->auction;
    component_seller_winning_bid_info.emplace();
    component_seller_winning_bid_info->auction_config =
        component_auction->config_;
    component_seller_winning_bid_info->bid =
        component_auction->top_bid_->bid->bid;
    component_seller_winning_bid_info->score =
        component_auction->top_bid_->score;
    component_seller_winning_bid_info->highest_scoring_other_bid =
        component_auction->highest_scoring_other_bid_;
    component_seller_winning_bid_info->highest_scoring_other_bid_owner =
        component_auction->highest_scoring_other_bid_owner_;
    component_seller_winning_bid_info->scoring_signals_data_version =
        component_auction->top_bid_->scoring_signals_data_version;
    component_seller_winning_bid_info->trace_id = component_auction->trace_id_;
    component_seller_winning_bid_info->component_auction_modified_bid_params =
        component_auction->top_bid_->component_auction_modified_bid_params
            ->Clone();
  }

  reporting_phase_callback_ = std::move(reporting_phase_callback);
  reporter_ = std::make_unique<InterestGroupAuctionReporter>(
      auction_worklet_manager_, std::move(winning_bid_info),
      std::move(top_level_seller_winning_bid_info),
      std::move(component_seller_winning_bid_info),
      std::move(private_aggregation_requests_));
  reporter_->Start(base::BindOnce(
      &InterestGroupAuction::OnReportingPhaseComplete, base::Unretained(this)));
  // The seller worklet handle is no longer needed. It's useful to keep it
  // alive until this point so that the InterestGroupAuctionReporter can reuse
  // it.
  seller_worklet_handle_.reset();
}

void InterestGroupAuction::ClosePipes() {
  // Release any worklets the reporter is keeping alive.
  reporter_.reset();

  // This is needed in addition to closing worklet pipes since the callbacks
  // passed to Mojo pipes this class doesn't own aren't cancellable.
  weak_ptr_factory_.InvalidateWeakPtrs();

  score_ad_receivers_.Clear();

  for (auto& buyer_helper : buyer_helpers_) {
    buyer_helper->ClosePipes();
  }
  seller_worklet_handle_.reset();

  // Close pipes for component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->ClosePipes();
  }
}

size_t InterestGroupAuction::NumPotentialBidders() const {
  size_t num_interest_groups = 0;
  for (const auto& buyer_helper : buyer_helpers_) {
    num_interest_groups += buyer_helper->num_potential_bidders();
  }
  for (auto& component_auction : component_auctions_) {
    num_interest_groups += component_auction->NumPotentialBidders();
  }
  return num_interest_groups;
}

void InterestGroupAuction::GetInterestGroupsThatBid(
    blink::InterestGroupSet& interest_groups) const {
  if (!all_bids_scored_)
    return;

  for (auto& buyer_helper : buyer_helpers_) {
    buyer_helper->GetInterestGroupsThatBid(interest_groups);
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->GetInterestGroupsThatBid(interest_groups);
  }
}

base::StringPiece GetRejectReasonString(
    const auction_worklet::mojom::RejectReason reject_reason) {
  base::StringPiece reject_reason_str;
  switch (reject_reason) {
    case auction_worklet::mojom::RejectReason::kNotAvailable:
      reject_reason_str = "not-available";
      break;
    case auction_worklet::mojom::RejectReason::kInvalidBid:
      reject_reason_str = "invalid-bid";
      break;
    case auction_worklet::mojom::RejectReason::kBidBelowAuctionFloor:
      reject_reason_str = "bid-below-auction-floor";
      break;
    case auction_worklet::mojom::RejectReason::kPendingApprovalByExchange:
      reject_reason_str = "pending-approval-by-exchange";
      break;
    case auction_worklet::mojom::RejectReason::kDisapprovedByExchange:
      reject_reason_str = "disapproved-by-exchange";
      break;
    case auction_worklet::mojom::RejectReason::kBlockedByPublisher:
      reject_reason_str = "blocked-by-publisher";
      break;
    case auction_worklet::mojom::RejectReason::kLanguageExclusions:
      reject_reason_str = "language-exclusions";
      break;
    case auction_worklet::mojom::RejectReason::kCategoryExclusions:
      reject_reason_str = "category-exclusions";
      break;
  }
  return reject_reason_str;
}

GURL InterestGroupAuction::FillPostAuctionSignals(
    const GURL& url,
    const PostAuctionSignals& signals,
    const absl::optional<PostAuctionSignals>& top_level_signals,
    const absl::optional<auction_worklet::mojom::RejectReason> reject_reason) {
  // TODO(qingxinwu): Round `winning_bid` and `highest_scoring_other_bid` to two
  // most-significant digits. Maybe same to corresponding browser signals of
  // reportWin()/reportResult().
  if (!url.has_query())
    return url;

  std::string query_string = url.query();
  base::ReplaceSubstringsAfterOffset(&query_string, 0, "${winningBid}",
                                     base::NumberToString(signals.winning_bid));

  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${madeWinningBid}",
      signals.made_winning_bid ? "true" : "false");
  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${highestScoringOtherBid}",
      base::NumberToString(signals.highest_scoring_other_bid));
  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${madeHighestScoringOtherBid}",
      signals.made_highest_scoring_other_bid ? "true" : "false");

  // For component auction sellers only, which get post auction signals from
  // both their own component auctions and top-level auction.
  // For now, we're assuming top-level auctions to be first-price auction only
  // (not second-price auction) and it does not need highest_scoring_other_bid.
  if (top_level_signals.has_value()) {
    base::ReplaceSubstringsAfterOffset(
        &query_string, 0, "${topLevelWinningBid}",
        base::NumberToString(top_level_signals->winning_bid));
    base::ReplaceSubstringsAfterOffset(
        &query_string, 0, "${topLevelMadeWinningBid}",
        top_level_signals->made_winning_bid ? "true" : "false");
  }

  if (reject_reason.has_value()) {
    base::ReplaceSubstringsAfterOffset(
        &query_string, 0, "${rejectReason}",
        GetRejectReasonString(reject_reason.value()));
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  return url.ReplaceComponents(replacements);
}

void InterestGroupAuction::TakeDebugReportUrls(
    std::vector<GURL>& debug_win_report_urls,
    std::vector<GURL>& debug_loss_report_urls) {
  if (!all_bids_scored_)
    return;

  // Set `winner` to the BidState in this auction associated with the winning
  // bid of the top-level auction, if there is one.
  //
  // In a component auction, the highest bid may have lost the top-level
  // auction, and we want to report that as a loss. In this case, AuctionResult
  // will be kComponentLostAuction.
  //
  // Also for the top-level auction in the case a component auctions bid won,
  // the highest bid's BidState and its reporting URLs are stored with the
  // component auction, so the component auction will be the one to populate
  // `debug_win_report_urls`.
  BidState* winner = nullptr;
  if (final_auction_result_ == AuctionResult::kSuccess &&
      top_bid_->bid->auction == this) {
    winner = top_bid_->bid->bid_state;
  }

  // `signals` includes post auction signals from current auction.
  PostAuctionSignals signals;
  signals.winning_bid = top_bid_ ? top_bid_->bid->bid : 0.0;
  signals.highest_scoring_other_bid = highest_scoring_other_bid_;
  // `top_level_signals` includes post auction signals from top-level auction.
  // Will only will be used in debug report URLs of top-level seller and
  // component sellers.
  // For now, we're assuming top-level auctions to be first-price auction only
  // (not second-price auction) and it does not need highest_scoring_other_bid.
  absl::optional<PostAuctionSignals> top_level_signals;
  if (parent_) {
    top_level_signals = PostAuctionSignals();
    top_level_signals->winning_bid =
        parent_->top_bid_ ? parent_->top_bid_->bid->bid : 0.0;
  }

  if (!top_bid_) {
    DCHECK_EQ(highest_scoring_other_bid_, 0);
    DCHECK(!highest_scoring_other_bid_owner_.has_value());
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    const url::Origin& owner = buyer_helper->owner();
    if (top_bid_)
      signals.made_winning_bid = owner == top_bid_->bid->interest_group->owner;

    if (highest_scoring_other_bid_owner_.has_value()) {
      DCHECK_GT(highest_scoring_other_bid_, 0);
      signals.made_highest_scoring_other_bid =
          owner == highest_scoring_other_bid_owner_.value();
    }
    if (parent_ && parent_->top_bid_) {
      top_level_signals->made_winning_bid =
          owner == parent_->top_bid_->bid->interest_group->owner;
    }

    buyer_helper->TakeDebugReportUrls(winner, signals, top_level_signals,
                                      debug_win_report_urls,
                                      debug_loss_report_urls);
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->TakeDebugReportUrls(debug_win_report_urls,
                                           debug_loss_report_urls);
  }
}

std::vector<GURL> InterestGroupAuction::TakeReportUrls() {
  return std::move(report_urls_);
}

std::map<url::Origin, InterestGroupAuction::PrivateAggregationRequests>
InterestGroupAuction::TakePrivateAggregationRequests() {
  for (auto& component_auction : component_auctions_) {
    std::map<url::Origin, PrivateAggregationRequests> requests_map =
        component_auction->TakePrivateAggregationRequests();
    for (auto& [origin, requests] : requests_map) {
      DCHECK(!requests.empty());
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_[origin];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
  return std::move(private_aggregation_requests_);
}

std::vector<std::string> InterestGroupAuction::TakeErrors() {
  for (auto& component_auction : component_auctions_) {
    std::vector<std::string> errors = component_auction->TakeErrors();
    errors_.insert(errors_.begin(), errors.begin(), errors.end());
  }
  return std::move(errors_);
}

void InterestGroupAuction::TakePostAuctionUpdateOwners(
    std::vector<url::Origin>& owners) {
  for (const url::Origin& owner : post_auction_update_owners_) {
    owners.emplace_back(std::move(owner));
  }

  for (auto& component_auction : component_auctions_) {
    component_auction->TakePostAuctionUpdateOwners(owners);
  }
}

InterestGroupAuction::ScoredBid* InterestGroupAuction::top_bid() {
  DCHECK(all_bids_scored_);
  DCHECK(top_bid_);
  return top_bid_.get();
}

absl::optional<uint16_t> InterestGroupAuction::GetBuyerExperimentId(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  auto it = config.per_buyer_experiment_group_ids.find(buyer);
  if (it != config.per_buyer_experiment_group_ids.end())
    return it->second;
  return config.all_buyer_experiment_group_id;
}

absl::optional<std::string> InterestGroupAuction::GetPerBuyerSignals(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  const auto& auction_config_per_buyer_signals =
      config.non_shared_params.per_buyer_signals;
  if (auction_config_per_buyer_signals.has_value()) {
    auto it = auction_config_per_buyer_signals.value().find(buyer);
    if (it != auction_config_per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

void InterestGroupAuction::OnInterestGroupRead(
    std::vector<StorageInterestGroup> interest_groups) {
  ++num_owners_loaded_;
  if (interest_groups.empty()) {
    OnOneLoadCompleted();
    return;
  }
  post_auction_update_owners_.push_back(
      interest_groups[0].interest_group.owner);
  for (const auto& bidder : interest_groups) {
    // Report freshness metrics.
    if (bidder.interest_group.daily_update_url.has_value()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Ads.InterestGroup.Auction.GroupFreshness.WithDailyUpdates",
          (base::Time::Now() - bidder.last_updated).InMinutes(),
          kGroupFreshnessMin.InMinutes(), kGroupFreshnessMax.InMinutes(),
          kGroupFreshnessBuckets);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Ads.InterestGroup.Auction.GroupFreshness.NoDailyUpdates",
          (base::Time::Now() - bidder.last_updated).InMinutes(),
          kGroupFreshnessMin.InMinutes(), kGroupFreshnessMax.InMinutes(),
          kGroupFreshnessBuckets);
    }
  }

  // Ignore interest groups with no bidding script or no ads.
  interest_groups.erase(
      std::remove_if(interest_groups.begin(), interest_groups.end(),
                     [](const StorageInterestGroup& bidder) {
                       return !bidder.interest_group.bidding_url ||
                              !bidder.interest_group.ads ||
                              bidder.interest_group.ads->empty();
                     }),
      interest_groups.end());

  // If there are no interest groups with both a bidding script and ads,
  // nothing else to do.
  if (interest_groups.empty()) {
    OnOneLoadCompleted();
    return;
  }

  ++num_owners_with_interest_groups_;

  auto buyer_helper =
      std::make_unique<BuyerHelper>(this, std::move(interest_groups));

  // BuyerHelper may filter out additional interest groups on construction.
  if (buyer_helper->has_potential_bidder()) {
    buyer_helpers_.emplace_back(std::move(buyer_helper));
  } else {
    // `buyer_helper` has a raw pointer to `this`, so if it's not added to
    // buyer_helpers_, delete it now to avoid a dangling pointer, since
    // OnOneLoadCompleted() could result in deleting `this`.
    buyer_helper.reset();
  }

  OnOneLoadCompleted();
}

void InterestGroupAuction::OnComponentInterestGroupsRead(
    AuctionList::iterator component_auction,
    bool success) {
  num_owners_loaded_ += (*component_auction)->num_owners_loaded_;
  num_owners_with_interest_groups_ +=
      (*component_auction)->num_owners_with_interest_groups_;

  // Erase component auctions that failed to load anything, so they won't be
  // invoked in the generate bid phase. This is not a problem in the reporting
  // phase, as the top-level auction knows which component auction, if any, won.
  if (!success)
    component_auctions_.erase(component_auction);
  OnOneLoadCompleted();
}

void InterestGroupAuction::OnOneLoadCompleted() {
  DCHECK_GT(num_pending_loads_, 0u);
  --num_pending_loads_;

  // Wait for more buyers to be loaded, if there are still some pending.
  if (num_pending_loads_ > 0)
    return;

  // Record histograms about the interest groups participating in the auction.
  // TODO(mmenke): Record histograms for component auctions.
  if (!parent_) {
    // Only record histograms if there were interest groups that could
    // theoretically participate in the auction.
    if (num_owners_loaded_ > 0) {
      size_t num_interest_groups = NumPotentialBidders();
      size_t num_sellers_with_bidders = component_auctions_.size();

      // If the top-level seller either has interest groups itself, or any of
      // the component auctions do, then the top-level seller also has bidders.
      if (num_interest_groups > 0)
        ++num_sellers_with_bidders;

      UMA_HISTOGRAM_COUNTS_1000("Ads.InterestGroup.Auction.NumInterestGroups",
                                num_interest_groups);
      UMA_HISTOGRAM_COUNTS_100(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          num_owners_with_interest_groups_);

      UMA_HISTOGRAM_COUNTS_100(
          "Ads.InterestGroup.Auction.NumSellersWithBidders",
          num_sellers_with_bidders);
    }
  }

  // If there are no potential bidders in this auction and no component auctions
  // with bidders, either, fail the auction.
  if (buyer_helpers_.empty() && component_auctions_.empty()) {
    OnStartLoadInterestGroupsPhaseComplete(AuctionResult::kNoInterestGroups);
    return;
  }

  // There are bidders that can generate bids, so complete without a final
  // result.
  OnStartLoadInterestGroupsPhaseComplete(
      /*auction_result=*/AuctionResult::kSuccess);
}

void InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete(
    AuctionResult auction_result) {
  DCHECK(load_interest_groups_phase_callback_);
  DCHECK(!final_auction_result_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "load_groups_phase", trace_id_);

  // `final_auction_result_` should only be set to kSuccess when the entire
  // auction is complete.
  bool success = auction_result == AuctionResult::kSuccess;
  if (!success)
    final_auction_result_ = auction_result;
  std::move(load_interest_groups_phase_callback_).Run(success);
}

void InterestGroupAuction::OnComponentSellerWorkletReceived() {
  DCHECK_GT(pending_component_seller_worklet_requests_, 0u);
  --pending_component_seller_worklet_requests_;
  if (pending_component_seller_worklet_requests_ == 0)
    RequestSellerWorklet();
}

void InterestGroupAuction::RequestSellerWorklet() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "request_seller_worklet",
                                    trace_id_);
  if (auction_worklet_manager_->RequestSellerWorklet(
          config_->decision_logic_url, config_->trusted_scoring_signals_url,
          config_->seller_experiment_group_id,
          base::BindOnce(&InterestGroupAuction::OnSellerWorkletReceived,
                         base::Unretained(this)),
          base::BindOnce(&InterestGroupAuction::OnSellerWorkletFatalError,
                         base::Unretained(this)),
          seller_worklet_handle_)) {
    OnSellerWorkletReceived();
  }
}

void InterestGroupAuction::OnSellerWorkletReceived() {
  DCHECK(!seller_worklet_received_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "request_seller_worklet",
                                  trace_id_);

  if (on_seller_receiver_callback_)
    std::move(on_seller_receiver_callback_).Run();

  seller_worklet_received_ = true;

  auto unscored_bids = std::move(unscored_bids_);
  for (auto& unscored_bid : unscored_bids) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "Wait_for_seller_worklet",
                                    *unscored_bid->bid_state->trace_id);
    ScoreBidIfReady(std::move(unscored_bid));
  }
  // No more unscored bids should be added, once the seller worklet has been
  // received.
  DCHECK(unscored_bids_.empty());
}

void InterestGroupAuction::OnSellerWorkletFatalError(
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

  OnBiddingAndScoringComplete(result, errors);
}

void InterestGroupAuction::OnComponentAuctionComplete(
    InterestGroupAuction* component_auction,
    bool success) {
  if (success) {
    // Create a copy of component Auction's bid, replacing values as necessary.
    const Bid* component_bid = component_auction->top_bid()->bid.get();
    const auto* modified_bid_params =
        component_auction->top_bid()
            ->component_auction_modified_bid_params.get();
    DCHECK(modified_bid_params);

    // Create a new event for the bid, since the component auction's event for
    // it ended after the component auction scored the bid.
    component_bid->bid_state->BeginTracing();

    ScoreBidIfReady(std::make_unique<Bid>(
        modified_bid_params->ad,
        modified_bid_params->has_bid ? modified_bid_params->bid
                                     : component_bid->bid,
        component_bid->render_url, component_bid->ad_components,
        component_bid->bid_duration,
        component_bid->bidding_signals_data_version, component_bid->bid_ad,
        component_bid->bid_state, component_bid->auction));
  }
  OnBidSourceDone();
}

void InterestGroupAuction::OnBidSourceDone() {
  --outstanding_bid_sources_;

  // If this is the only bid that yet to be sent to the seller worklet, and
  // the seller worklet has loaded, then tell the seller worklet to send any
  // pending scoring signals request to complete the auction more quickly.
  if (outstanding_bid_sources_ == 0 && seller_worklet_received_)
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();

  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::ScoreBidIfReady(std::unique_ptr<Bid> bid) {
  DCHECK(bid);
  DCHECK(bid->bid_state->made_bid);

  any_bid_made_ = true;

  // If seller worklet hasn't been received yet, wait until it is.
  if (!seller_worklet_received_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_for_seller_worklet",
                                      *bid->bid_state->trace_id);
    unscored_bids_.emplace_back(std::move(bid));
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "fledge", "seller_worklet_score_ad", *bid->bid_state->trace_id,
      "decision_logic_url", config_->decision_logic_url);

  ++bids_being_scored_;
  Bid* bid_raw = bid.get();

  mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient> score_ad_remote;
  score_ad_receivers_.Add(
      this, score_ad_remote.InitWithNewPipeAndPassReceiver(), std::move(bid));
  seller_worklet_handle_->GetSellerWorklet()->ScoreAd(
      bid_raw->ad_metadata, bid_raw->bid, config_->non_shared_params,
      GetOtherSellerParam(*bid_raw), bid_raw->interest_group->owner,
      bid_raw->render_url, bid_raw->ad_components,
      bid_raw->bid_duration.InMilliseconds(), SellerTimeout(),
      *bid_raw->bid_state->trace_id, std::move(score_ad_remote));
}

bool InterestGroupAuction::ValidateScoreBidCompleteResult(
    double score,
    auction_worklet::mojom::ComponentAuctionModifiedBidParams*
        component_auction_modified_bid_params,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url) {
  // If `debug_loss_report_url` or `debug_win_report_url` is not a valid HTTPS
  // URL, the auction should fail because the worklet is compromised.
  if (debug_loss_report_url.has_value() &&
      !IsUrlValid(debug_loss_report_url.value())) {
    score_ad_receivers_.ReportBadMessage(
        "Invalid seller debugging loss report URL");
    return false;
  }
  if (debug_win_report_url.has_value() &&
      !IsUrlValid(debug_win_report_url.value())) {
    score_ad_receivers_.ReportBadMessage(
        "Invalid seller debugging win report URL");
    return false;
  }

  // Only validate `component_auction_modified_bid_params` if the bid was
  // accepted.
  if (score > 0) {
    // If they accept a bid / return a positive score, component auction
    // SellerWorklets must return a `component_auction_modified_bid_params`,
    // and top-level auctions must not.
    if ((parent_ == nullptr) !=
        (component_auction_modified_bid_params == nullptr)) {
      score_ad_receivers_.ReportBadMessage(
          "Invalid component_auction_modified_bid_params");
      return false;
    }
    // If a component seller modified the bid, the new bid must also be valid.
    if (component_auction_modified_bid_params &&
        component_auction_modified_bid_params->has_bid &&
        !IsValidBid(component_auction_modified_bid_params->bid)) {
      score_ad_receivers_.ReportBadMessage(
          "Invalid component_auction_modified_bid_params bid");
      return false;
    }
  }
  return true;
}

void InterestGroupAuction::OnScoreAdComplete(
    double score,
    auction_worklet::mojom::RejectReason reject_reason,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    uint32_t data_version,
    bool has_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    const std::vector<std::string>& errors) {
  DCHECK_GT(bids_being_scored_, 0);

  if (!ValidateScoreBidCompleteResult(
          score, component_auction_modified_bid_params.get(),
          debug_loss_report_url, debug_win_report_url)) {
    OnBiddingAndScoringComplete(AuctionResult::kBadMojoMessage);
    return;
  }

  std::unique_ptr<Bid> bid = std::move(score_ad_receivers_.current_context());
  score_ad_receivers_.Remove(score_ad_receivers_.current_receiver());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "seller_worklet_score_ad",
                                  *bid->bid_state->trace_id);
  bid->bid_state->EndTracing();

  --bids_being_scored_;

  // The mojom API declaration should ensure none of these are null.
  DCHECK(base::ranges::none_of(
      pa_requests,
      [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
             request_ptr) { return request_ptr.is_null(); }));
  if (!pa_requests.empty()) {
    DCHECK(config_);
    PrivateAggregationRequests& pa_requests_for_seller =
        private_aggregation_requests_[config_->seller];
    pa_requests_for_seller.insert(pa_requests_for_seller.end(),
                                  std::move_iterator(pa_requests.begin()),
                                  std::move_iterator(pa_requests.end()));
  }

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Use separate fields for component and top-level seller reports, so both can
  // send debug reports.
  if (bid->auction == this) {
    bid->bid_state->seller_debug_loss_report_url =
        std::move(debug_loss_report_url);
    bid->bid_state->seller_debug_win_report_url =
        std::move(debug_win_report_url);
    // Ignores reject reason if score > 0.
    if (score <= 0)
      bid->bid_state->reject_reason = reject_reason;
  } else {
    bid->bid_state->top_level_seller_debug_loss_report_url =
        std::move(debug_loss_report_url);
    bid->bid_state->top_level_seller_debug_win_report_url =
        std::move(debug_win_report_url);
  }

  // A score <= 0 means the seller rejected the bid.
  if (score <= 0) {
    // Need to delete `bid` because OnBiddingAndScoringComplete() may delete
    // this, which leaves danging pointers on the stack. While this is safe to
    // do (nothing has access to `bid` to dereference them), it makes the
    // dangling pointer tooling sad.
    bid.reset();
    MaybeCompleteBiddingAndScoringPhase();
    return;
  }

  bool is_top_bid = false;
  const url::Origin& owner = bid->interest_group->owner;

  if (!top_bid_ || score > top_bid_->score) {
    // If there's no previous top bidder, or the bidder has the highest score,
    // need to replace the previous top bidder.
    is_top_bid = true;
    if (top_bid_) {
      OnNewHighestScoringOtherBid(top_bid_->score, top_bid_->bid->bid,
                                  &top_bid_->bid->interest_group->owner);
    }
    num_top_bids_ = 1;
    at_most_one_top_bid_owner_ = true;
  } else if (score == top_bid_->score) {
    // If there's a tie, replace the top-bidder with 1-in-`num_top_bids_`
    // chance. This is the select random value from a stream with fixed
    // storage problem.
    ++num_top_bids_;
    if (1 == base::RandInt(1, num_top_bids_))
      is_top_bid = true;
    if (owner != top_bid_->bid->interest_group->owner)
      at_most_one_top_bid_owner_ = false;
    // If the top bid is being replaced, need to add the old top bid as a second
    // highest bid. Otherwise, need to add the current bid as a second highest
    // bid.
    double new_highest_scoring_other_bid =
        is_top_bid ? top_bid_->bid->bid : bid->bid;
    OnNewHighestScoringOtherBid(
        score, new_highest_scoring_other_bid,
        at_most_one_top_bid_owner_ ? &bid->interest_group->owner : nullptr);
  } else if (score >= second_highest_score_) {
    // Also use this bid (the most recent one) as highest scoring other bid if
    // there's a tie for second highest score.
    OnNewHighestScoringOtherBid(score, bid->bid, &owner);
  }

  if (is_top_bid) {
    top_bid_ = std::make_unique<ScoredBid>(
        score, has_data_version ? data_version : absl::optional<uint32_t>(),
        std::move(bid), std::move(component_auction_modified_bid_params));
  }

  bid.reset();
  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::OnNewHighestScoringOtherBid(
    double score,
    double bid_value,
    const url::Origin* owner) {
  // Current (the most recent) bid becomes highest scoring other bid.
  if (score > second_highest_score_) {
    highest_scoring_other_bid_ = bid_value;
    num_second_highest_bids_ = 1;
    // Owner may be false if this is one of the bids tied for first place.
    if (!owner) {
      highest_scoring_other_bid_owner_.reset();
    } else {
      highest_scoring_other_bid_owner_ = *owner;
    }
    second_highest_score_ = score;
    return;
  }

  DCHECK_EQ(score, second_highest_score_);
  if (!owner || *owner != highest_scoring_other_bid_owner_)
    highest_scoring_other_bid_owner_.reset();
  ++num_second_highest_bids_;
  // In case of a tie, randomly pick one. This is the select random value from a
  // stream with fixed storage problem.
  if (1 == base::RandInt(1, num_second_highest_bids_))
    highest_scoring_other_bid_ = bid_value;
}

absl::optional<std::string> InterestGroupAuction::PerBuyerSignals(
    const BidState* state) {
  const auto& per_buyer_signals = config_->non_shared_params.per_buyer_signals;
  if (per_buyer_signals.has_value()) {
    auto it =
        per_buyer_signals.value().find(state->bidder.interest_group.owner);
    if (it != per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

absl::optional<base::TimeDelta> InterestGroupAuction::PerBuyerTimeout(
    const BidState* state) {
  const auto& per_buyer_timeouts =
      config_->non_shared_params.per_buyer_timeouts;
  if (per_buyer_timeouts.has_value()) {
    auto it =
        per_buyer_timeouts.value().find(state->bidder.interest_group.owner);
    if (it != per_buyer_timeouts.value().end())
      return std::min(it->second, kMaxTimeout);
  }
  const auto& all_buyers_timeout =
      config_->non_shared_params.all_buyers_timeout;
  if (all_buyers_timeout.has_value())
    return std::min(all_buyers_timeout.value(), kMaxTimeout);
  return absl::nullopt;
}

absl::optional<base::TimeDelta> InterestGroupAuction::SellerTimeout() {
  if (config_->non_shared_params.seller_timeout.has_value()) {
    return std::min(config_->non_shared_params.seller_timeout.value(),
                    kMaxTimeout);
  }
  return absl::nullopt;
}

void InterestGroupAuction::MaybeCompleteBiddingAndScoringPhase() {
  if (!AllBidsScored())
    return;

  all_bids_scored_ = true;

  // If there's no winning bid, fail with kAllBidsRejected if there were any
  // bids. Otherwise, fail with kNoBids.
  if (!top_bid_) {
    if (any_bid_made_) {
      OnBiddingAndScoringComplete(AuctionResult::kAllBidsRejected);
    } else {
      OnBiddingAndScoringComplete(AuctionResult::kNoBids);
    }
    return;
  }

  OnBiddingAndScoringComplete(AuctionResult::kSuccess);
}

void InterestGroupAuction::OnBiddingAndScoringComplete(
    AuctionResult auction_result,
    const std::vector<std::string>& errors) {
  DCHECK(bidding_and_scoring_phase_callback_);
  DCHECK(!final_auction_result_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidding_and_scoring_phase",
                                  trace_id_);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // If this is a component auction, have to unload the seller worklet handle to
  // avoid deadlock. Otherwise, loading the top-level seller worklet may be
  // blocked by component seller worklets taking up all the quota.
  if (parent_)
    seller_worklet_handle_.reset();

  // If the seller loaded callback hasn't been invoked yet, call it now. This is
  // needed in the case the phase ended without receiving the seller worklet
  // (e.g., in the case no bidder worklet bids).
  if (on_seller_receiver_callback_)
    std::move(on_seller_receiver_callback_).Run();

  bool success = auction_result == AuctionResult::kSuccess;
  if (!success) {
    // Close all pipes, to prevent any pending callbacks from being invoked if
    // this phase is being completed due to a fatal error, like the seller
    // worklet failing to load.
    ClosePipes();

    // `final_auction_result_` should only be set to kSuccess when the entire
    // auction is complete.
    final_auction_result_ = auction_result;
  }

  // If this is a top-level auction with component auction, update final state
  // of all successfully completed component auctions with bids that did not win
  // to reflect a loss.
  for (auto& component_auction : component_auctions_) {
    // Leave the state of the winning component auction alone, if the winning
    // bid is from a component auction.
    if (top_bid_ && top_bid_->bid->auction == component_auction.get())
      continue;
    if (component_auction->final_auction_result_)
      continue;
    component_auction->final_auction_result_ =
        AuctionResult::kComponentLostAuction;
  }

  std::move(bidding_and_scoring_phase_callback_).Run(success);
}

void InterestGroupAuction::OnReportingPhaseComplete() {
  DCHECK(reporting_phase_callback_);
  DCHECK(!final_auction_result_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "reporting_phase", trace_id_);

  // Extract all results from the reporter, and then destroy it.
  errors_.insert(errors_.end(), reporter_->errors().begin(),
                 reporter_->errors().end());
  private_aggregation_requests_ = reporter_->TakePrivateAggregationRequests();
  ad_beacon_map_ = reporter_->TakeAdBeaconMap();
  report_urls_ = reporter_->TakeReportUrls();
  reporter_.reset();

  final_auction_result_ = AuctionResult::kSuccess;
  // If there's a winning bid, set its auction result as well. If the winning
  // bid came from a component auction, this will set that component auction's
  // result as well. This is needed for auction result accessors.
  //
  // TODO(mmenke): Extract relevant data from `this` when creating the Reporter,
  // and have it handle reporting only if auction results are loaded in a frame,
  // or if there's no result.
  if (top_bid_)
    top_bid_->bid->auction->final_auction_result_ = AuctionResult::kSuccess;

  // Close all pipes, as they're no longer needed.
  ClosePipes();

  std::move(reporting_phase_callback_).Run(/*success=*/true);
}

auction_worklet::mojom::ComponentAuctionOtherSellerPtr
InterestGroupAuction::GetOtherSellerParam(const Bid& bid) const {
  auction_worklet::mojom::ComponentAuctionOtherSellerPtr
      browser_signals_other_seller;
  if (parent_) {
    // This is a component seller scoring a bid from its own auction.
    // Need to provide the top-level seller origin.
    browser_signals_other_seller =
        auction_worklet::mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
            parent_->config_->seller);
  } else if (bid.auction != this) {
    // This is a top-level seller scoring a bid from a component auction.
    // Need to provide the component seller origin.
    browser_signals_other_seller =
        auction_worklet::mojom::ComponentAuctionOtherSeller::NewComponentSeller(
            bid.auction->config_->seller);
  }
  return browser_signals_other_seller;
}

bool InterestGroupAuction::RequestBidderWorklet(
    BidState& bid_state,
    base::OnceClosure worklet_available_callback,
    AuctionWorkletManager::FatalErrorCallback fatal_error_callback) {
  DCHECK(!bid_state.worklet_handle);

  const blink::InterestGroup& interest_group = bid_state.bidder.interest_group;

  absl::optional<uint16_t> experiment_group_id =
      GetBuyerExperimentId(*config_, interest_group.owner);

  return auction_worklet_manager_->RequestBidderWorklet(
      interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url, experiment_group_id,
      std::move(worklet_available_callback), std::move(fatal_error_callback),
      bid_state.worklet_handle);
}

}  // namespace content
