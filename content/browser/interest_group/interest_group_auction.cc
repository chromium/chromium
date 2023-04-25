// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_auction.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_result.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
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
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxPerBuyerTimeout = base::Milliseconds(500);

// For group freshness metrics.
constexpr base::TimeDelta kGroupFreshnessMin = base::Minutes(1);
constexpr base::TimeDelta kGroupFreshnessMax = base::Days(30);
constexpr int kGroupFreshnessBuckets = 100;

// All URLs received from worklets must be valid HTTPS URLs. It's up to callers
// to call ReportBadMessage() on invalid URLs.
bool IsUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

bool IsKAnon(const base::flat_map<std::string, bool>& kanon_keys,
             const std::string& key) {
  auto it = kanon_keys.find(key);
  return it != kanon_keys.end() && it->second;
}

bool IsKAnon(const base::flat_map<std::string, bool>& kanon_keys,
             const blink::InterestGroup& interest_group,
             const auction_worklet::mojom::BidderWorkletBid* bid) {
  if (!IsKAnon(kanon_keys,
               blink::KAnonKeyForAdBid(interest_group, bid->ad_descriptor))) {
    return false;
  }
  if (bid->ad_component_descriptors.has_value()) {
    for (const blink::AdDescriptor& component :
         bid->ad_component_descriptors.value()) {
      if (!IsKAnon(kanon_keys, blink::KAnonKeyForAdComponentBid(component))) {
        return false;
      }
    }
  }
  return true;
}

base::flat_map<auction_worklet::mojom::KAnonKeyPtr, bool> KAnonKeysToMojom(
    const base::flat_map<std::string, bool>& kanon_keys) {
  std::vector<std::pair<auction_worklet::mojom::KAnonKeyPtr, bool>> result;
  for (const auto& key : kanon_keys) {
    result.emplace_back(auction_worklet::mojom::KAnonKey::New(key.first),
                        key.second);
  }
  return std::move(result);
}

// Finds InterestGroup::Ad in `ads` that matches `ad_descriptor`, if any.
// Returns nullptr if `ad_descriptor` is invalid.
const blink::InterestGroup::Ad* FindMatchingAd(
    const std::vector<blink::InterestGroup::Ad>& ads,
    const base::flat_map<std::string, bool>& kanon_keys,
    const blink::InterestGroup& interest_group,
    InterestGroupAuction::Bid::BidRole bid_role,
    bool is_component_ad,
    const blink::AdDescriptor& ad_descriptor) {
  // TODO(mmenke): Validate render URLs on load and make this a DCHECK just
  // before the return instead, since then `ads` will necessarily only contain
  // valid URLs at that point.
  if (!IsUrlValid(ad_descriptor.url)) {
    return nullptr;
  }

  if (ad_descriptor.size && !IsValidAdSize(ad_descriptor.size.value())) {
    return nullptr;
  }

  if (bid_role != InterestGroupAuction::Bid::BidRole::kUnenforcedKAnon) {
    const std::string kanon_key =
        is_component_ad
            ? blink::KAnonKeyForAdComponentBid(ad_descriptor)
            : blink::KAnonKeyForAdBid(interest_group, ad_descriptor);
    if (!IsKAnon(kanon_keys, kanon_key)) {
      return nullptr;
    }
  }

  for (const auto& ad : ads) {
    if (ad.render_url != ad_descriptor.url) {
      continue;
    }
    if (!ad.size_group && !ad_descriptor.size) {
      // Neither `blink::InterestGroup::Ad` nor the ad from the bid have any
      // size specifications. They are considered as matching ad as long as
      // they have the same url.
      return &ad;
    }
    if (!ad.size_group || !ad_descriptor.size) {
      // Since only one of the ads has a size specification, they are considered
      // not matching.
      continue;
    }
    // Both `blink::InterestGroup::Ad` and the ad from the bid have size
    // specifications. They are considered as matching ad only if their
    // size also matches.
    auto has_matching_ad_size = [&interest_group,
                                 &ad_descriptor](const std::string& ad_size) {
      return interest_group.ad_sizes->at(ad_size) == *ad_descriptor.size;
    };
    if (base::ranges::any_of(interest_group.size_groups->at(ad.size_group),
                             has_matching_ad_size)) {
      // Each size group may also correspond to multiple ad sizes. If any of
      // those ad sizes matches with the ad size from `ad_descriptor`, they are
      // considered as matching ads.
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
    return std::tie(a->calculated_priority, a->bidder->joining_origin,
                    a->bidder->interest_group.execution_mode) >
           std::tie(b->calculated_priority, b->bidder->joining_origin,
                    b->bidder->interest_group.execution_mode);
  }
};

bool IsBidRoleUsedForWinner(
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    InterestGroupAuction::Bid::BidRole bid_role) {
  if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kEnforce) {
    return bid_role != InterestGroupAuction::Bid::BidRole::kUnenforcedKAnon;
  } else {
    return bid_role != InterestGroupAuction::Bid::BidRole::kEnforcedKAnon;
  }
}

static const char* ScoreAdTraceEventName(const InterestGroupAuction::Bid& bid) {
  if (bid.bid_role == InterestGroupAuction::Bid::BidRole::kEnforcedKAnon) {
    return "seller_worklet_score_kanon_enforced_ad";
  } else {
    return "seller_worklet_score_ad";
  }
}

// Returns true iff `interest_group` grants `seller` all the capabilities in
// `capabilities`.
bool GroupSatisfiesAllCapabilities(const blink::InterestGroup& interest_group,
                                   blink::SellerCapabilitiesType capabilities,
                                   const url::Origin& seller) {
  if (interest_group.seller_capabilities) {
    auto it = interest_group.seller_capabilities->find(seller);
    if (it != interest_group.seller_capabilities->end()) {
      return it->second.HasAll(capabilities);
    }
  }
  return interest_group.all_sellers_capabilities.HasAll(capabilities);
}

// Helper for ReportPaBuyersValueIfAllowed() -- returns true iff
// `interest_group`'s seller capabilities has authorized `capability` for
// `seller`.
bool CanReportPaBuyersValue(const blink::InterestGroup& interest_group,
                            blink::SellerCapabilities capability,
                            const url::Origin& seller) {
  return GroupSatisfiesAllCapabilities(interest_group, {capability}, seller);
}

// Helper for ReportPaBuyersValueIfAllowed() -- returns the bucket base
// of `buyer`, if present in `config`'s `auction_report_buyer_keys`.
absl::optional<absl::uint128> BucketBaseForReportPaBuyers(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  if (!config.non_shared_params.auction_report_buyer_keys) {
    return absl::nullopt;
  }
  // Find the index of the buyer in `buyers`. It should be present, since we
  // only load interest groups belonging to owners from `buyers`.
  DCHECK(config.non_shared_params.interest_group_buyers);
  const std::vector<url::Origin>& buyers =
      *config.non_shared_params.interest_group_buyers;
  absl::optional<size_t> index;
  for (size_t i = 0; i < buyers.size(); i++) {
    if (buyer == buyers.at(i)) {
      index = i;
      break;
    }
  }
  DCHECK(index);
  // Use that index to get the associated bucket base, if present.
  if (*index >= config.non_shared_params.auction_report_buyer_keys->size()) {
    return absl::nullopt;
  }
  return config.non_shared_params.auction_report_buyer_keys->at(*index);
}

// Helper for ReportPaBuyersValueIfAllowed() -- returns the
// AuctionReportBuyersConfig for `buyer_report_type`, if it exists in
// `auction_report_buyers` in `config`.
absl::optional<blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>
ReportBuyersConfigForPaBuyers(
    blink::AuctionConfig::NonSharedParams::BuyerReportType buyer_report_type,
    const blink::AuctionConfig& config) {
  if (!config.non_shared_params.auction_report_buyers) {
    return absl::nullopt;
  }
  const auto& report_buyers = *config.non_shared_params.auction_report_buyers;
  auto it = report_buyers.find(buyer_report_type);
  if (it == report_buyers.end()) {
    return absl::nullopt;
  }
  return it->second;
}

// Retrieves the timeout from `buyer_timeouts` associated with `buyer`, if any.
// Used for both `buyer_timeouts` and `buyer_cumulative_timeouts`, stored in
// AuctionConfigs. Callers should use PerBuyerTimeout() and
// PerBuyerCumulativeTimeout() instead, since those apply the timeout limit,
// when applicable.
absl::optional<base::TimeDelta> PerBuyerTimeoutHelper(
    const url::Origin& buyer,
    const blink::AuctionConfig::MaybePromiseBuyerTimeouts& buyer_timeouts) {
  DCHECK(!buyer_timeouts.is_promise());
  const auto& per_buyer_timeouts = buyer_timeouts.value().per_buyer_timeouts;
  if (per_buyer_timeouts.has_value()) {
    auto it = per_buyer_timeouts->find(buyer);
    if (it != per_buyer_timeouts->end()) {
      return it->second;
    }
  }
  const auto& all_buyers_timeout = buyer_timeouts.value().all_buyers_timeout;
  if (all_buyers_timeout.has_value()) {
    return all_buyers_timeout.value();
  }
  return absl::nullopt;
}

absl::optional<base::TimeDelta> PerBuyerTimeout(
    const url::Origin& buyer,
    const blink::AuctionConfig& auction_config) {
  absl::optional<base::TimeDelta> out = PerBuyerTimeoutHelper(
      buyer, auction_config.non_shared_params.buyer_timeouts);
  if (!out) {
    return out;
  }
  return std::min(*out, kMaxPerBuyerTimeout);
}

absl::optional<base::TimeDelta> PerBuyerCumulativeTimeout(
    const url::Origin& buyer,
    const blink::AuctionConfig& auction_config) {
  return PerBuyerTimeoutHelper(
      buyer, auction_config.non_shared_params.buyer_cumulative_timeouts);
}

absl::optional<blink::AdCurrency> PerBuyerCurrency(
    const url::Origin& buyer,
    const blink::AuctionConfig& auction_config) {
  const blink::AuctionConfig::MaybePromiseBuyerCurrencies& buyer_currencies =
      auction_config.non_shared_params.buyer_currencies;
  DCHECK(!buyer_currencies.is_promise());
  const auto& per_buyer_currencies =
      buyer_currencies.value().per_buyer_currencies;
  if (per_buyer_currencies.has_value()) {
    auto it = per_buyer_currencies->find(buyer);
    if (it != per_buyer_currencies->end()) {
      return it->second;
    }
  }
  const auto& all_buyers_currency =
      buyer_currencies.value().all_buyers_currency;
  return all_buyers_currency;  // Maybe nullopt.
}

}  // namespace

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals() = default;

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals(
    double winning_bid,
    absl::optional<blink::AdCurrency> winning_bid_currency,
    bool made_winning_bid)
    : winning_bid(winning_bid),
      winning_bid_currency(std::move(winning_bid_currency)),
      made_winning_bid(made_winning_bid) {}

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals(
    double winning_bid,
    absl::optional<blink::AdCurrency> winning_bid_currency,
    bool made_winning_bid,
    double highest_scoring_other_bid,
    absl::optional<blink::AdCurrency> highest_scoring_other_bid_currency,
    bool made_highest_scoring_other_bid)
    : winning_bid(winning_bid),
      winning_bid_currency(std::move(winning_bid_currency)),
      made_winning_bid(made_winning_bid),
      highest_scoring_other_bid(highest_scoring_other_bid),
      highest_scoring_other_bid_currency(
          std::move(highest_scoring_other_bid_currency)),
      made_highest_scoring_other_bid(made_highest_scoring_other_bid) {}

InterestGroupAuction::PostAuctionSignals::~PostAuctionSignals() = default;

// static
void InterestGroupAuction::PostAuctionSignals::FillWinningBidInfo(
    const url::Origin& owner,
    absl::optional<url::Origin> winner_owner,
    double winning_bid,
    absl::optional<double> winning_bid_in_seller_currency,
    const absl::optional<blink::AdCurrency>& seller_currency,
    bool& out_made_winning_bid,
    double& out_winning_bid,
    absl::optional<blink::AdCurrency>& out_winning_bid_currency) {
  out_made_winning_bid = false;
  if (winner_owner.has_value()) {
    out_made_winning_bid = owner == *winner_owner;
  }

  if (seller_currency.has_value()) {
    out_winning_bid = winning_bid_in_seller_currency.value_or(0.0);
    out_winning_bid_currency = *seller_currency;
  } else {
    out_winning_bid = winning_bid;
    out_winning_bid_currency = absl::nullopt;
  }
}

// static
void InterestGroupAuction::PostAuctionSignals::
    FillRelevantHighestScoringOtherBidInfo(
        const url::Origin& owner,
        absl::optional<url::Origin> highest_scoring_other_bid_owner,
        double highest_scoring_other_bid,
        absl::optional<double> highest_scoring_other_bid_in_seller_currency,
        const absl::optional<blink::AdCurrency>& seller_currency,
        bool& out_made_highest_scoring_other_bid,
        double& out_highest_scoring_other_bid,
        absl::optional<blink::AdCurrency>&
            out_highest_scoring_other_bid_currency) {
  out_made_highest_scoring_other_bid = false;
  if (highest_scoring_other_bid_owner.has_value()) {
    DCHECK_GT(highest_scoring_other_bid, 0);
    out_made_highest_scoring_other_bid =
        owner == highest_scoring_other_bid_owner.value();
  }

  if (seller_currency.has_value()) {
    out_highest_scoring_other_bid =
        highest_scoring_other_bid_in_seller_currency.value_or(0);
    out_highest_scoring_other_bid_currency = *seller_currency;
  } else {
    out_highest_scoring_other_bid = highest_scoring_other_bid;
    out_highest_scoring_other_bid_currency = absl::nullopt;
  }
}

InterestGroupAuction::BidState::BidState() = default;

InterestGroupAuction::BidState::~BidState() {
  if (trace_id.has_value()) {
    EndTracing();
  }
  if (trace_id_for_kanon_scoring.has_value()) {
    EndTracingKAnonScoring();
  }
}

InterestGroupAuction::BidState::BidState(BidState&&) = default;

InterestGroupAuction::BidState& InterestGroupAuction::BidState::operator=(
    BidState&&) = default;

void InterestGroupAuction::BidState::BeginTracing() {
  DCHECK(!trace_id.has_value());

  trace_id = base::trace_event::GetNextGlobalTraceId();

  const blink::InterestGroup& interest_group = bidder->interest_group;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("fledge", "bid", *trace_id, "bidding_url",
                                    interest_group.bidding_url,
                                    "interest_group_name", interest_group.name);
}

void InterestGroupAuction::BidState::EndTracing() {
  DCHECK(trace_id.has_value());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bid", *trace_id);
  trace_id = absl::nullopt;
}

void InterestGroupAuction::BidState::BeginTracingKAnonScoring() {
  DCHECK(!trace_id_for_kanon_scoring.has_value());

  trace_id_for_kanon_scoring = base::trace_event::GetNextGlobalTraceId();

  const blink::InterestGroup& interest_group = bidder->interest_group;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("fledge", "score_kanon_enforced_ad",
                                    *trace_id_for_kanon_scoring, "bidding_url",
                                    interest_group.bidding_url,
                                    "interest_group_name", interest_group.name);
}

void InterestGroupAuction::BidState::EndTracingKAnonScoring() {
  DCHECK(trace_id_for_kanon_scoring.has_value());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "score_kanon_enforced_ad",
                                  *trace_id_for_kanon_scoring);
  trace_id_for_kanon_scoring = absl::nullopt;
}

InterestGroupAuction::Bid::Bid(
    BidRole bid_role,
    std::string ad_metadata,
    double bid,
    absl::optional<blink::AdCurrency> bid_currency,
    absl::optional<double> ad_cost,
    blink::AdDescriptor ad_descriptor,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    absl::optional<uint16_t> modeling_signals,
    base::TimeDelta bid_duration,
    absl::optional<uint32_t> bidding_signals_data_version,
    const blink::InterestGroup::Ad* bid_ad,
    BidState* bid_state,
    InterestGroupAuction* auction)
    : bid_role(bid_role),
      ad_metadata(std::move(ad_metadata)),
      bid(bid),
      bid_currency(std::move(bid_currency)),
      ad_cost(std::move(ad_cost)),
      ad_descriptor(std::move(ad_descriptor)),
      ad_component_descriptors(std::move(ad_component_descriptors)),
      modeling_signals(modeling_signals),
      bid_duration(bid_duration),
      bidding_signals_data_version(bidding_signals_data_version),
      interest_group(&bid_state->bidder->interest_group),
      bid_ad(bid_ad),
      bid_state(bid_state),
      auction(auction) {
  DCHECK(IsValidBid(bid));
}

InterestGroupAuction::Bid::Bid(Bid&) = default;

InterestGroupAuction::Bid::~Bid() = default;

std::vector<GURL> InterestGroupAuction::Bid::GetAdComponentUrls() const {
  std::vector<GURL> ad_component_urls;
  ad_component_urls.reserve(ad_component_descriptors.size());
  base::ranges::transform(
      ad_component_descriptors, std::back_inserter(ad_component_urls),
      [](const blink::AdDescriptor& ad_component_descriptor) {
        return ad_component_descriptor.url;
      });
  return ad_component_urls;
}

InterestGroupAuction::ScoredBid::ScoredBid(
    double score,
    absl::optional<uint32_t> scoring_signals_data_version,
    std::unique_ptr<Bid> bid,
    absl::optional<double> bid_in_seller_currency,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params)
    : score(score),
      scoring_signals_data_version(scoring_signals_data_version),
      bid(std::move(bid)),
      bid_in_seller_currency(std::move(bid_in_seller_currency)),
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
        if (priority < 0) {
          auction_->auction_metrics_recorder_
              ->RecordBidFilteredDuringInterestGroupLoad();
          continue;
        }
      }

      if (bidder.interest_group.enable_bidding_signals_prioritization) {
        enable_bidding_signals_prioritization_ = true;
      }

      auto state = std::make_unique<BidState>();
      state->bidder = std::make_unique<StorageInterestGroup>(std::move(bidder));
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

    // Figure out which BidState is last for each key, as it will be responsible
    // for sending out the trusted bidder signals request.
    std::set<AuctionWorkletManager::WorkletKey> seen_keys;
    for (auto it = bid_states_.rbegin(); it != bid_states_.rend(); ++it) {
      std::unique_ptr<BidState>& bid_state = *it;
      auto [iter, success] =
          seen_keys.insert(auction_->BidderWorkletKey(*bid_state));
      bid_state->send_pending_trusted_signals_after_generate_bid = success;
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
    start_generating_bids_time_ = base::TimeTicks::Now();

    // Request processes for all bidder worklets.
    for (auto& bid_state : bid_states_) {
      auto worklet_key = auction_->BidderWorkletKey(*bid_state);
      auction_->auction_metrics_recorder_->ReportBidderWorkletKey(worklet_key);
      auction_->auction_worklet_manager_->RequestWorkletByKey(
          worklet_key,
          base::BindOnce(&BuyerHelper::OnBidderWorkletReceived,
                         base::Unretained(this), bid_state.get()),
          base::BindOnce(&BuyerHelper::OnBidderWorkletGenerateBidFatalError,
                         base::Unretained(this), bid_state.get()),
          bid_state->worklet_handle);
    }
  }

  // auction_worklet::mojom::GenerateBidClient implementation:

  void OnBiddingSignalsReceived(
      const base::flat_map<std::string, double>& priority_vector,
      base::TimeDelta trusted_signals_fetch_latency,
      base::OnceClosure resume_generate_bid_callback) override {
    BidState* state = generate_bid_client_receiver_set_.current_context();
    const blink::InterestGroup& interest_group = state->bidder->interest_group;
    auction_->ReportTrustedSignalsFetchLatency(interest_group,
                                               trusted_signals_fetch_latency);
    absl::optional<double> new_priority;
    if (!priority_vector.empty()) {
      new_priority = CalculateInterestGroupPriority(
          *auction_->config_, *state->bidder, auction_->auction_start_time_,
          priority_vector,
          (interest_group.priority_vector &&
           !interest_group.priority_vector->empty())
              ? state->calculated_priority
              : absl::optional<double>());
      if (*new_priority < 0) {
        auction_->auction_metrics_recorder_
            ->RecordBidFilteredDuringReprioritization();
      }
    }
    OnBiddingSignalsReceivedInternal(state, new_priority,
                                     std::move(resume_generate_bid_callback));
  }

  void OnGenerateBidComplete(
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_kanon_bid,
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
      PrivateAggregationRequests non_kanon_pa_requests,
      base::TimeDelta bidding_latency,
      const std::vector<std::string>& errors) override {
    BidState* state = generate_bid_client_receiver_set_.current_context();
    const blink::InterestGroup& interest_group = state->bidder->interest_group;
    auction_->ReportBiddingLatency(interest_group, bidding_latency);

    // This is intentionally recorded here as opposed to in
    // OnGenerateBidCompleteInternal in order to exclude bids that were
    // filtered during reprioritization. It also excludes those bids that
    // encountered a fatal error, except for timeouts; those we record to this
    // metric separately and explicitly in OnTimeout.
    auction_->auction_metrics_recorder_->RecordBidForOneInterestGroupLatency(
        base::TimeTicks::Now() - start_generating_bids_time_);
    OnGenerateBidCompleteInternal(
        state, std::move(mojo_bid), std::move(mojo_kanon_bid),
        bidding_signals_data_version, has_bidding_signals_data_version,
        debug_loss_report_url, debug_win_report_url, set_priority,
        has_set_priority, std::move(update_priority_signals_overrides),
        std::move(pa_requests), std::move(non_kanon_pa_requests), errors);
  }

  // Closes all Mojo pipes, releases all weak pointers, and stops the timeout
  // timer.
  void ClosePipes() {
    weak_ptr_factory_.InvalidateWeakPtrs();

    for (auto& bid_state : bid_states_) {
      CloseBidStatePipes(*bid_state);
    }
    // No need to clear `generate_bid_client_receiver_set_`, since
    // CloseBidStatePipes() should take care of that.
    DCHECK(generate_bid_client_receiver_set_.empty());

    // Need to stop the timer - this is called on completion and on certain
    // errors. Don't want the timer to trigger anything after there's been a
    // failure already.
    cumulative_buyer_timeout_timer_.Stop();
  }

  // Returns true if this buyer has any interest groups that will potentially
  // bid in an auction -- that is, not all interest groups have been filtered
  // out.
  bool has_potential_bidder() const { return !bid_states_.empty(); }

  size_t num_potential_bidders() const { return bid_states_.size(); }

  const url::Origin& owner() const { return owner_; }

  void GetInterestGroupsThatBidAndReportBidCounts(
      blink::InterestGroupSet& interest_groups) const {
    size_t bid_count = 0;
    for (const auto& bid_state : bid_states_) {
      if (bid_state->made_bid) {
        interest_groups.emplace(bid_state->bidder->interest_group.owner,
                                bid_state->bidder->interest_group.name);
        bid_count++;
      }
    }
    for (const auto& bid_state : bid_states_) {
      if (auction_->ReportBidCount(bid_state->bidder->interest_group,
                                   bid_count)) {
        break;
      }
    }
  }

  // Adds debug reporting URLs to `debug_win_report_urls` and
  // `debug_loss_report_urls`, if there are any, filling in report URL template
  // parameters as needed.
  //
  // `winner` points to the BidState associated with the winning bid, if there
  // is one. If it's not a BidState managed by `this`, it has no effect.
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
    for (std::unique_ptr<BidState>& bid_state : bid_states_) {
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
        // and made_highest_scoring_other_bid signals. (And also the currency
        // bit for those).
        debug_loss_report_urls.emplace_back(FillPostAuctionSignals(
            std::move(bid_state->bidder_debug_loss_report_url).value(),
            PostAuctionSignals(
                signals.winning_bid, signals.winning_bid_currency,
                signals.made_winning_bid, /*highest_scoring_other_bid=*/0.0,
                /*highest_scoring_other_bid_currency=*/absl::nullopt,
                /*made_highest_scoring_other_bid=*/false),
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

  // Returns private aggregation requests, if there are any. Calculate
  // bucket/value using `signals` as needed.
  //
  // `winner` points to the BidState associated with the winning bid, if there
  // is one. If it's not a BidState managed by `this`, it has no effect.
  //
  // `signals` are the PostAuctionSignals from the auction `this` was a part of.
  void TakePrivateAggregationRequests(
      const BidState* winner,
      const BidState* non_kanon_winner,
      const PostAuctionSignals& signals,
      const absl::optional<PostAuctionSignals>& top_level_signals,
      std::map<url::Origin, PrivateAggregationRequests>&
          private_aggregation_requests_reserved,
      std::map<std::string, PrivateAggregationRequests>&
          private_aggregation_requests_non_reserved) {
    for (std::unique_ptr<BidState>& state : bid_states_) {
      bool is_winner = state.get() == winner;
      for (auto& [key, requests] : state->private_aggregation_requests) {
        const url::Origin& origin = key.first;
        bool is_top_level_seller = key.second;
        double winning_bid_to_use = signals.winning_bid;
        double highest_scoring_other_bid_to_use =
            signals.highest_scoring_other_bid;
        // When component auctions are in use, a BuyerHelper for a component
        // auction calls here for the scoreAd() aggregation calls from the
        // top-level; in that case the relevant signals are in
        // `top_level_signals` and not `signals`. `highest_scoring_other_bid`
        // is also not reported for top-levels.
        if (is_top_level_seller && auction_->parent_) {
          highest_scoring_other_bid_to_use = 0;
          winning_bid_to_use = top_level_signals.has_value()
                                   ? top_level_signals->winning_bid
                                   : 0.0;
        }

        for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
             requests) {
          absl::optional<PrivateAggregationRequestWithEventType>
              converted_request = FillInPrivateAggregationRequest(
                  std::move(request), winning_bid_to_use,
                  highest_scoring_other_bid_to_use, state->reject_reason,
                  is_winner);
          if (converted_request.has_value()) {
            PrivateAggregationRequestWithEventType converted_request_value =
                std::move(converted_request.value());
            const absl::optional<std::string>& event_type =
                converted_request_value.event_type;
            if (event_type.has_value()) {
              // The request has a non-reserved event type.
              private_aggregation_requests_non_reserved[event_type.value()]
                  .emplace_back(std::move(converted_request_value.request));
            } else {
              private_aggregation_requests_reserved[origin].emplace_back(
                  std::move(converted_request_value.request));
            }
          }
        }
      }
      if (non_kanon_winner == state.get()) {
        const url::Origin& bidder = state->bidder->interest_group.owner;
        for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
             state->non_kanon_private_aggregation_requests) {
          absl::optional<PrivateAggregationRequestWithEventType>
              converted_request = FillInPrivateAggregationRequest(
                  std::move(request), signals.winning_bid,
                  signals.highest_scoring_other_bid,
                  auction_worklet::mojom::RejectReason::kBelowKAnonThreshold,
                  false);
          if (converted_request.has_value()) {
            PrivateAggregationRequestWithEventType converted_request_value =
                std::move(converted_request.value());
            // Only reserved types are supported for k-anon failures.
            // This *should* be guaranteed by `FillInPrivateAggregationRequest`
            // since we passed in `false` for `is_winner`.
            DCHECK(!converted_request_value.event_type.has_value());
            private_aggregation_requests_reserved[bidder].emplace_back(
                std::move(converted_request_value.request));
          }
        }
      }
    }
  }

  void NotifyConfigPromisesResolved() {
    DCHECK(auction_->config_promises_resolved_);

    // If there are no outstanding bids, just do nothing. It's safest to exit
    // early in the case that bidder worklet process crashed or failed to fetch
    // the necessary script(s) before all config promises were resolved, rather
    // than rely on everything handling that case correctly.
    if (num_outstanding_bids_ == 0) {
      return;
    }

    MaybeStartCumulativeTimeoutTimer();

    for (const auto& bid_state : bid_states_) {
      FinishGenerateBidIfReady(bid_state.get());
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
    auction_->auction_metrics_recorder_->RecordBidsFilteredByPerBuyerLimits(
        bid_states_.size() - size_limit_);
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
    auction_->auction_metrics_recorder_
        ->RecordBidAbortedByBidderWorkletFatalError();

    // Add error(s) directly to error list.
    if (fatal_error_type ==
        AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
      // Ignore default error message in case of crash. Instead, use a more
      // specific one.
      OnFatalError(
          bid_state,
          {base::StrCat({bid_state->bidder->interest_group.bidding_url->spec(),
                         " crashed while trying to run generateBid()."})});
    } else {
      OnFatalError(bid_state, errors);
    }
  }

  // Called in the case of a fatal error that prevents the `bid_state` worklet
  // from bidding.
  void OnFatalError(BidState* bid_state, std::vector<std::string> errors) {
    auction_->errors_.insert(auction_->errors_.end(),
                             std::make_move_iterator(errors.begin()),
                             std::make_move_iterator(errors.end()));

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
    OnGenerateBidCompleteInternal(
        bid_state,
        /*mojo_bid=*/auction_worklet::mojom::BidderWorkletBidPtr(),
        /*mojo_kanon_bid=*/
        auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
        /*bidding_signals_data_version=*/0,
        /*has_bidding_signals_data_version=*/false,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*set_priority=*/0,
        /*has_set_priority=*/false,
        /*update_priority_signals_overrides=*/{},
        /*pa_requests=*/{},
        /*non_kanon_pa_requests=*/{},
        /*errors=*/{});
  }

  base::flat_map<std::string, bool> ComputeKAnon(
      const StorageInterestGroup& storage_interest_group,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode) {
    if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kNone) {
      return {};
    }

    // k-anon cache is always checked against the same time, to avoid weird
    // behavior of validity changing in the middle of the auction.
    base::Time start_time = auction_->auction_start_time_;

    std::vector<std::pair<std::string, bool>> kanon_entries;
    for (const auto& ad_kanon : storage_interest_group.bidding_ads_kanon) {
      if (IsKAnonymous(ad_kanon, start_time)) {
        kanon_entries.emplace_back(ad_kanon.key, true);
      }
    }
    for (const auto& component_ad_kanon :
         storage_interest_group.component_ads_kanon) {
      if (IsKAnonymous(component_ad_kanon, start_time)) {
        kanon_entries.emplace_back(component_ad_kanon.key, true);
      }
    }
    return base::flat_map<std::string, bool>(std::move(kanon_entries));
  }

  // Invoked whenever the AuctionWorkletManager has provided a BidderWorket
  // for the bidder identified by `bid_state`. Starts generating a bid.
  void OnBidderWorkletReceived(BidState* bid_state) {
    if (!bidder_process_received_) {
      // All bidder worklets are expected to be loaded in the same process, so
      // as soon as any worklet has been received, can set this to true.
      bidder_process_received_ = true;
      MaybeStartCumulativeTimeoutTimer();
    }

    const blink::InterestGroup& interest_group =
        bid_state->bidder->interest_group;

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
        auction_->kanon_mode();
    bid_state->kanon_keys = ComputeKAnon(*bid_state->bidder, kanon_mode);
    SubresourceUrlBuilder* url_builder =
        auction_->SubresourceUrlBuilderIfReady();
    if (url_builder) {
      bid_state->worklet_handle->AuthorizeSubresourceUrls(*url_builder);
      bid_state->handled_direct_from_seller_signals_in_begin_generate_bid =
          true;
    }
    bid_state->worklet_handle->GetBidderWorklet()->BeginGenerateBid(
        auction_worklet::mojom::BidderWorkletNonSharedParams::New(
            interest_group.name,
            interest_group.enable_bidding_signals_prioritization,
            interest_group.priority_vector, interest_group.execution_mode,
            interest_group.update_url,
            interest_group.trusted_bidding_signals_keys,
            interest_group.user_bidding_signals, interest_group.ads,
            interest_group.ad_components,
            KAnonKeysToMojom(bid_state->kanon_keys)),
        kanon_mode, bid_state->bidder->joining_origin,
        GetDirectFromSellerPerBuyerSignals(
            url_builder, bid_state->bidder->interest_group.owner),
        GetDirectFromSellerAuctionSignals(url_builder),
        auction_->config_->seller,
        auction_->parent_ ? auction_->parent_->config_->seller
                          : absl::optional<url::Origin>(),
        bid_state->bidder->bidding_browser_signals.Clone(),
        auction_->auction_start_time_, *bid_state->trace_id,
        std::move(pending_remote),
        bid_state->bid_finalizer.BindNewEndpointAndPassReceiver());

    // TODO(morlovich): This should arguably be merged into BeginGenerateBid
    // for footprint; check how testable that would be.
    if (bid_state->send_pending_trusted_signals_after_generate_bid) {
      bid_state->worklet_handle->GetBidderWorklet()
          ->SendPendingSignalsRequests();
    }

    FinishGenerateBidIfReady(bid_state);
  }

  void FinishGenerateBidIfReady(BidState* bid_state) {
    if (!auction_->config_promises_resolved_) {
      return;
    }

    if (!bid_state->bid_finalizer.is_bound()) {
      // This can happen if the promise resolves while the worklet process is
      // still being launched.
      return;
    }

    SubresourceUrlBuilder* url_builder =
        auction_->SubresourceUrlBuilderIfReady();
    if (url_builder) {
      if (bid_state->handled_direct_from_seller_signals_in_begin_generate_bid) {
        // Don't provide direct-from-seller info to FinishGenerateBid below
        // since we already provided it to BeginGenerateBid.
        url_builder = nullptr;
      } else {
        bid_state->worklet_handle->AuthorizeSubresourceUrls(*url_builder);
      }
    }
    bid_state->bid_finalizer->FinishGenerateBid(
        auction_->config_->non_shared_params.auction_signals.value(),
        GetPerBuyerSignals(*auction_->config_,
                           bid_state->bidder->interest_group.owner),
        PerBuyerTimeout(owner_, *auction_->config_),
        PerBuyerCurrency(owner_, *auction_->config_),
        GetDirectFromSellerPerBuyerSignals(
            url_builder, bid_state->bidder->interest_group.owner),
        GetDirectFromSellerAuctionSignals(url_builder));
    bid_state->bid_finalizer.reset();
  }

  // Invoked when OnBiddingSignalsReceived() has been called for `state`, or
  // with a negative priority when the worklet process has an error, or the
  // buyer reaches their cumulative timeout, and is still waiting on the
  // OnBiddingSignalsReceived() invocation.
  void OnBiddingSignalsReceivedInternal(
      BidState* state,
      absl::optional<double> new_priority,
      base::OnceClosure resume_generate_bid_callback) {
    DCHECK(!state->bidding_signals_received);
    DCHECK_GT(num_outstanding_bids_, 0);
    DCHECK_GT(num_outstanding_bidding_signals_received_calls_, 0);
    // `resume_generate_bid_callback` must be non-null except when invoked with
    // a negative `net_priority` on worklet error.
    DCHECK(resume_generate_bid_callback || *new_priority < 0);

    state->bidding_signals_received = true;
    --num_outstanding_bidding_signals_received_calls_;

    // If `new_priority` has a value and is negative, need to record the bidder
    // as no longer participating in the auction and cancel bid generation.
    bool bid_filtered = new_priority.has_value() && *new_priority < 0;
    UMA_HISTOGRAM_BOOLEAN("Ads.InterestGroup.Auction.BidFiltered",
                          bid_filtered);
    if (bid_filtered) {
      // Record if there are other bidders, as if there are not, the next call
      // may delete `this`.
      bool other_bidders = (num_outstanding_bids_ > 1);

      // If the result of applying the filter is negative, complete the bid
      // with OnGenerateBidCompleteInternal(), which will close the relevant
      // pipes and abort bid generation.
      OnGenerateBidCompleteInternal(
          state, /*mojo_bid=*/auction_worklet::mojom::BidderWorkletBidPtr(),
          /*mojo_kanon_bid=*/
          auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr(),
          /*bidding_signals_data_version=*/0,
          /*has_bidding_signals_data_version=*/false,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          /*set_priority=*/0,
          /*has_set_priority=*/false,
          /*update_priority_signals_overrides=*/{},
          /*pa_requests=*/{},
          /*non_kanon_pa_requests=*/{},
          /*errors=*/{});
      // If this was the last bidder, and it was filtered out, there's nothing
      // else to do, and `this` may have already been deleted.
      if (!other_bidders) {
        return;
      }

      // If bidding_signals_prioritization is not enabled, there's also
      // nothing else to do - no other bidders were blocked on the bidder's
      // OnBiddingSignalsReceived() call.
      if (!enable_bidding_signals_prioritization_) {
        return;
      }
    } else {
      if (new_priority.has_value()) {
        state->calculated_priority = *new_priority;
      }
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
    if (num_outstanding_bidding_signals_received_calls_ > 0) {
      return;
    }

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
      auction_worklet::mojom::BidderWorkletKAnonEnforcedBidPtr mojo_kanon_bid,
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
      PrivateAggregationRequests non_kanon_pa_requests,
      const std::vector<std::string>& errors) {
    DCHECK(!state->made_bid);
    DCHECK_GT(num_outstanding_bids_, 0);

    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidder_worklet_generate_bid",
                                    *state->trace_id);

    const blink::InterestGroup& interest_group = state->bidder->interest_group;
    absl::optional<uint32_t> maybe_bidding_signals_data_version;
    if (has_bidding_signals_data_version) {
      maybe_bidding_signals_data_version = bidding_signals_data_version;
    }

    if (has_set_priority) {
      auction_->interest_group_manager_->SetInterestGroupPriority(
          blink::InterestGroupKey(interest_group.owner, interest_group.name),
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
            blink::InterestGroupKey(interest_group.owner, interest_group.name),
            std::move(update_priority_signals_overrides));
      }
    }

    // Validate `mojo_kanon_bid` coming in from the less-trusted worklet
    // process; k-anonymity itself will be checked by TryToCreateBid.
    if (mojo_kanon_bid) {
      if (auction_->kanon_mode_ ==
          auction_worklet::mojom::KAnonymityBidMode::kNone) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Received k-anon bid data when not considering k-anon");
        mojo_kanon_bid.reset();
      } else if (mojo_bid &&
                 IsKAnon(state->kanon_keys, interest_group, mojo_bid.get())) {
        if (!mojo_kanon_bid->is_same_as_non_enforced()) {
          generate_bid_client_receiver_set_.ReportBadMessage(
              "Received different k-anon bid when unenforced bid already "
              "k-anon");
          mojo_kanon_bid = auction_worklet::mojom::
              BidderWorkletKAnonEnforcedBid::NewSameAsNonEnforced(nullptr);
        }
      }
    }

    // The mojom API declaration should ensure none of these are null.
    DCHECK(base::ranges::none_of(
        pa_requests,
        [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
               request_ptr) { return request_ptr.is_null(); }));
    DCHECK(base::ranges::none_of(
        non_kanon_pa_requests,
        [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
               request_ptr) { return request_ptr.is_null(); }));
    auction_->MaybeLogPrivateAggregationWebFeatures(pa_requests);
    if (!pa_requests.empty()) {
      PrivateAggregationRequests& pa_requests_for_bidder =
          state->private_aggregation_requests[std::make_pair(
              interest_group.owner, false /*not a top-level seller*/)];
      pa_requests_for_bidder.insert(pa_requests_for_bidder.end(),
                                    std::move_iterator(pa_requests.begin()),
                                    std::move_iterator(pa_requests.end()));
    }
    if (!non_kanon_pa_requests.empty()) {
      PrivateAggregationRequests& non_kanon_pa_requests_for_bidder =
          state->non_kanon_private_aggregation_requests;
      non_kanon_pa_requests_for_bidder.insert(
          non_kanon_pa_requests_for_bidder.end(),
          std::move_iterator(non_kanon_pa_requests.begin()),
          std::move_iterator(non_kanon_pa_requests.end()));
    }

    auction_->errors_.insert(auction_->errors_.end(), errors.begin(),
                             errors.end());

    // Ignore invalid bids.
    std::unique_ptr<Bid> bid;
    std::string ad_metadata;
    // `mojo_bid` is null if the worklet doesn't bid, or if the bidder worklet
    // fails to load / crashes.
    if (mojo_bid) {
      // It's possible that k-anon enforced bid is the same as one with out
      // enforcement, in which case we make sure to only run ScoreBid once.
      Bid::BidRole role = Bid::BidRole::kUnenforcedKAnon;
      if (mojo_kanon_bid) {
        if (mojo_kanon_bid->is_same_as_non_enforced()) {
          role = Bid::BidRole::kBothKAnonModes;
          auction_->auction_metrics_recorder_
              ->RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
        } else {
          auction_->auction_metrics_recorder_
              ->RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();
        }
      } else {
        auction_->auction_metrics_recorder_
            ->RecordInterestGroupWithOnlyNonKAnonBid();
      }
      bid = TryToCreateBid(role, std::move(mojo_bid), *state,
                           maybe_bidding_signals_data_version,
                           debug_loss_report_url, debug_win_report_url);
      if (bid) {
        state->bidder_debug_loss_report_url = debug_loss_report_url;
      }
    } else {
      // Bidders who do not bid are allowed to get loss report.
      state->bidder_debug_loss_report_url = debug_loss_report_url;
      auction_->auction_metrics_recorder_->RecordInterestGroupWithNoBids();
    }

    std::unique_ptr<Bid> kanon_bid;
    if (mojo_kanon_bid && !mojo_kanon_bid->is_same_as_non_enforced()) {
      kanon_bid = TryToCreateBid(Bid::BidRole::kEnforcedKAnon,
                                 std::move(mojo_kanon_bid->get_bid()), *state,
                                 maybe_bidding_signals_data_version,
                                 /*debug_loss_report_url=*/absl::nullopt,
                                 /*debug_win_report_url=*/absl::nullopt);
    }

    // Release the worklet. If it wins the auction, it will be requested again
    // to invoke its ReportWin() method.
    CloseBidStatePipes(*state);

    if (!bid && !kanon_bid) {
      if (state->trace_id.has_value()) {
        // Might not have started it if we timed out before worklet received.
        state->EndTracing();
      }
    } else {
      state->bidder_debug_win_report_url = debug_win_report_url;
      state->made_bid = true;
      if (bid) {
        auction_->ScoreBidIfReady(std::move(bid));
      }
      if (kanon_bid) {
        state->BeginTracingKAnonScoring();
        auction_->ScoreBidIfReady(std::move(kanon_bid));
      }
    }

    --num_outstanding_bids_;
    if (num_outstanding_bids_ == 0) {
      DCHECK_EQ(num_outstanding_bidding_signals_received_calls_, 0);
      // Pipes should already be closed at this point, but the
      // `cumulative_buyer_timeout_timer_` needs to be stopped if it's running,
      // and it's safest to keep all logic to stop everything `this` may be
      // doing in one place.
      ClosePipes();

      auction_->OnBidSourceDone();
    }
  }

  void MaybeStartCumulativeTimeoutTimer() {
    // This should only be called when there are outstanding bids.
    DCHECK_GT(num_outstanding_bids_, 0);

    // Do nothing if still waiting on the seller to provide more of the
    // AuctionConfig, or waiting on a process to be assigned (which would mean
    // that this may be waiting behind other buyers).
    if (!auction_->config_promises_resolved_ || !bidder_process_received_) {
      return;
    }

    DCHECK(!cumulative_buyer_timeout_timer_.IsRunning());

    // Get cumulative buyer timeout. Note that this must be done after the
    // `config_promises_resolved_` check above.
    absl::optional<base::TimeDelta> cumulative_buyer_timeout =
        PerBuyerCumulativeTimeout(owner_, *auction_->config_);

    // Nothing to do if there's no cumulative timeout.
    if (!cumulative_buyer_timeout) {
      return;
    }

    cumulative_buyer_timeout_timer_.Start(
        FROM_HERE, *cumulative_buyer_timeout,
        base::BindOnce(&BuyerHelper::OnTimeout, base::Unretained(this)));
  }

  // Called when the `cumulative_buyer_timeout_timer_` expires.
  void OnTimeout() {
    // If there are no outstanding bids, then the timer should not still be
    // running.
    DCHECK_GT(num_outstanding_bids_, 0);

    // Assemble a list of interest groups that haven't bid yet - have to do
    // this, since calling OnGenerateBidCompleteInternal() on the last
    // incomplete bid may delete `this`, if it ends the auction.
    std::list<BidState*> pending_bids;
    for (auto& bid_state : bid_states_) {
      if (!bid_state->worklet_handle) {
        continue;
      }
      // Put the IGs that have received signals first, since cancelling the last
      // bid that has not received signals could cause a bid that has received
      // signals to start running Javascript.
      if (bid_state->bidding_signals_received) {
        pending_bids.push_front(bid_state.get());
      } else {
        pending_bids.push_back(bid_state.get());
      }
    }

    auction_->auction_metrics_recorder_
        ->RecordBidsAbortedByBuyerCumulativeTimeout(pending_bids.size());
    for (auto* pending_bid : pending_bids) {
      // We specifically include timeouts in this metric.
      auction_->auction_metrics_recorder_->RecordBidForOneInterestGroupLatency(
          base::TimeTicks::Now() - start_generating_bids_time_);

      // Fail bids individually, with errors. This does potentially do extra
      // work over just failing the entire auction directly, but ensures there's
      // a single failure path, reducing the chance of future breakages.
      OnFatalError(
          pending_bid, /*errors=*/{base::StrCat(
              {pending_bid->bidder->interest_group.bidding_url->spec(),
               " perBuyerCumulativeTimeout exceeded during bid generation."})});
    }
  }

  // Validates that `mojo_bid` is valid and, if it is, creates a Bid
  // corresponding to it, consuming it. Returns nullptr and calls
  // ReportBadMessage() if it's not valid. Does not mutate `bid_state`, but
  // the returned Bid has a non-const pointer to it.
  std::unique_ptr<InterestGroupAuction::Bid> TryToCreateBid(
      InterestGroupAuction::Bid::BidRole bid_role,
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      BidState& bid_state,
      const absl::optional<uint32_t>& bidding_signals_data_version,
      const absl::optional<GURL>& debug_loss_report_url,
      const absl::optional<GURL>& debug_win_report_url) {
    // We record the bid duration even if the bid is invalid to avoid bias.
    auction_->auction_metrics_recorder_->RecordGenerateSingleBidLatency(
        mojo_bid->bid_duration);

    if (!IsValidBid(mojo_bid->bid)) {
      generate_bid_client_receiver_set_.ReportBadMessage("Invalid bid value");
      return nullptr;
    }

    if (mojo_bid->bid_duration.is_negative()) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bid duration");
      return nullptr;
    }

    if (!blink::VerifyAdCurrencyCode(
            PerBuyerCurrency(owner_, *auction_->config_),
            mojo_bid->bid_currency)) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bid currency");
      return nullptr;
    }

    const blink::InterestGroup& interest_group =
        bid_state.bidder->interest_group;
    const blink::InterestGroup::Ad* matching_ad = FindMatchingAd(
        *interest_group.ads, bid_state.kanon_keys, interest_group, bid_role,
        /*is_component_ad=*/false, mojo_bid->ad_descriptor);
    if (!matching_ad) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Bid render ad must have a valid URL and size (if specified)");
      return nullptr;
    }

    // Validate `ad_component` URLs, if present.
    std::vector<blink::AdDescriptor> ad_component_descriptors;
    if (mojo_bid->ad_component_descriptors) {
      // Only InterestGroups with ad components should return bids with ad
      // components.
      if (!interest_group.ad_components) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Unexpected non-null ad component list");
        return nullptr;
      }

      if (mojo_bid->ad_component_descriptors->size() >
          blink::kMaxAdAuctionAdComponents) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Too many ad component URLs");
        return nullptr;
      }

      // Validate each ad component URL is valid and appears in the interest
      // group's `ad_components` field.
      for (const blink::AdDescriptor& ad_component_descriptor :
           *mojo_bid->ad_component_descriptors) {
        if (!FindMatchingAd(*interest_group.ad_components, bid_state.kanon_keys,
                            interest_group, bid_role, /*is_component_ad=*/true,
                            ad_component_descriptor)) {
          generate_bid_client_receiver_set_.ReportBadMessage(
              "Bid ad component must have a valid URL and size (if specified)");
          return nullptr;
        }
      }
      ad_component_descriptors = *std::move(mojo_bid->ad_component_descriptors);
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
        bid_role, std::move(mojo_bid->ad), mojo_bid->bid,
        std::move(mojo_bid->bid_currency), mojo_bid->ad_cost,
        std::move(mojo_bid->ad_descriptor), std::move(ad_component_descriptors),
        std::move(mojo_bid->modeling_signals), mojo_bid->bid_duration,
        bidding_signals_data_version, matching_ad, &bid_state, auction_);
  }

  // Close all Mojo pipes associated with `state`.
  void CloseBidStatePipes(BidState& state) {
    state.worklet_handle.reset();
    if (state.generate_bid_client_receiver_id) {
      generate_bid_client_receiver_set_.Remove(
          *state.generate_bid_client_receiver_id);
      state.generate_bid_client_receiver_id.reset();
      state.bid_finalizer.reset();
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

  // Set to true once a single bidder worklet has been received (and thus, since
  // all bidder worklets managed by a BuyerHelper use the same process, `this`
  // is no longer blocked waiting on other bidders to complete).
  bool bidder_process_received_ = false;

  // Timer for applying the perBidderCumulativeTimeout, if one is applicable.
  // Starts once `bidder_process_received_` and
  // `auction_->config_promises_resolved_` are true, if
  // `cumulative_buyer_timeout_` is not nullopt.
  base::OneShotTimer cumulative_buyer_timeout_timer_;

  int num_outstanding_bidding_signals_received_calls_ = 0;
  int num_outstanding_bids_ = 0;

  // Records the time at which StartGeneratingBids was called for UKM.
  base::TimeTicks start_generating_bids_time_;

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
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    const blink::AuctionConfig* config,
    const InterestGroupAuction* parent,
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    AuctionMetricsRecorder* auction_metrics_recorder,
    base::Time auction_start_time,
    base::RepeatingCallback<
        void(const PrivateAggregationRequests& private_aggregation_requests)>
        maybe_log_private_aggregation_web_features_callback)
    : trace_id_(base::trace_event::GetNextGlobalTraceId()),
      kanon_mode_(kanon_mode),
      auction_worklet_manager_(auction_worklet_manager),
      interest_group_manager_(interest_group_manager),
      auction_metrics_recorder_(auction_metrics_recorder),
      config_(config),
      config_promises_resolved_(config_->NumPromises() == 0),
      parent_(parent),
      auction_start_time_(auction_start_time),
      creation_time_(base::TimeTicks::Now()),
      maybe_log_private_aggregation_web_features_callback_(
          std::move(maybe_log_private_aggregation_web_features_callback)) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("fledge", "auction", *trace_id_,
                                    "decision_logic_url",
                                    config_->decision_logic_url);

  uint32_t child_pos = 0;
  for (const auto& component_auction_config :
       config->non_shared_params.component_auctions) {
    // Nested component auctions are not supported.
    DCHECK(!parent_);
    component_auctions_.emplace(
        child_pos, std::make_unique<InterestGroupAuction>(
                       kanon_mode_, &component_auction_config, /*parent=*/this,
                       auction_worklet_manager, interest_group_manager,
                       auction_metrics_recorder_, auction_start_time,
                       maybe_log_private_aggregation_web_features_callback_));
    ++child_pos;
  }

  if (!parent_) {
    auction_metrics_recorder_->SetKAnonymityBidMode(kanon_mode);
  }
}

InterestGroupAuction::~InterestGroupAuction() {
  if (trace_id_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "auction", *trace_id_);
  }

  if (!final_auction_result_) {
    final_auction_result_ = AuctionResult::kAborted;
  }

  // TODO(mmenke): Record histograms for component auctions.
  if (!parent_) {
    UMA_HISTOGRAM_ENUMERATION("Ads.InterestGroup.Auction.Result",
                              *final_auction_result_);

    if (HasNonKAnonWinner()) {
      UMA_HISTOGRAM_BOOLEAN("Ads.InterestGroup.Auction.NonKAnonWinnerIsKAnon",
                            NonKAnonWinnerIsKAnon());
    }

    // Only record time of full auctions and aborts.
    switch (*final_auction_result_) {
      case AuctionResult::kAborted:
        UMA_HISTOGRAM_MEDIUM_TIMES("Ads.InterestGroup.Auction.AbortTime",
                                   base::TimeTicks::Now() - creation_time_);
        break;
      case AuctionResult::kNoBids:
      case AuctionResult::kAllBidsRejected:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
            base::TimeTicks::Now() - creation_time_);
        break;
      case AuctionResult::kSuccess:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
            base::TimeTicks::Now() - creation_time_);
        break;
      default:
        break;
    }

    // Last UKM we record for this auction. This finalizes and records the
    // AdsInterestGroup_AuctionLatency entry. Any further interactions with
    // auction_metrics_recorder_ will likely cause a CHECK-fail.
    auction_metrics_recorder_->OnAuctionEnd(*final_auction_result_);
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
  DCHECK(!final_auction_result_);
  DCHECK_EQ(num_pending_loads_, 0u);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "load_groups_phase", *trace_id_);

  load_interest_groups_phase_callback_ =
      std::move(load_interest_groups_phase_callback);

  // If the seller can't participate in the auction, fail the auction.
  if (!is_interest_group_api_allowed_callback.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          config_->seller)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
            weak_ptr_factory_.GetWeakPtr(), AuctionResult::kSellerRejected));
    return;
  }

  for (auto component_auction = component_auctions_.begin();
       component_auction != component_auctions_.end(); ++component_auction) {
    component_auction->second->StartLoadInterestGroupsPhase(
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
  DCHECK(!final_auction_result_);
  DCHECK(!non_kanon_enforced_auction_leader_.top_bid);
  DCHECK(!kanon_enforced_auction_leader_.top_bid);
  DCHECK_EQ(pending_component_seller_worklet_requests_, 0u);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "bidding_and_scoring_phase",
                                    *trace_id_);

  on_seller_receiver_callback_ = std::move(on_seller_receiver_callback);
  bidding_and_scoring_phase_callback_ =
      std::move(bidding_and_scoring_phase_callback);

  bidding_and_scoring_phase_start_time_ = base::TimeTicks::Now();

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
    for (auto& component_auction_info : component_auctions_) {
      InterestGroupAuction* component_auction =
          component_auction_info.second.get();
      component_auction->StartBiddingAndScoringPhase(
          base::BindOnce(
              &InterestGroupAuction::OnComponentSellerWorkletReceived,
              base::Unretained(this)),
          base::BindOnce(&InterestGroupAuction::OnComponentAuctionComplete,
                         base::Unretained(this), component_auction));
    }
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    buyer_helper->StartGeneratingBids();
  }
}

std::unique_ptr<InterestGroupAuctionReporter>
InterestGroupAuction::CreateReporter(
    AttributionManager* attribution_manager,
    PrivateAggregationManager* private_aggregation_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<blink::AuctionConfig> auction_config,
    const url::Origin& main_frame_origin,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    blink::InterestGroupSet interest_groups_that_bid) {
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK_EQ(*final_auction_result_, AuctionResult::kSuccess);
  DCHECK(non_kanon_enforced_auction_leader_.top_bid);

  // This should only be called on top-level auctions.
  DCHECK(!parent_);

  uint64_t trace_id = *trace_id_;
  trace_id_.reset();

  const LeaderInfo& leader = leader_info();
  InterestGroupAuction::ScoredBid* winner = leader.top_bid.get();
  InterestGroupAuctionReporter::WinningBidInfo winning_bid_info;
  // TODO(https://crbug.com/1394777): Moving `bidder` out of the winning bid
  // state is not very safe. We should modify the InterestGroupAuction API so
  // that values are returned directly to the AuctionRunner, instead of being
  // pulled out, and create the reporter last, to avoid and UAF issues with the
  // winning interest group.
  winning_bid_info.storage_interest_group =
      std::move(winner->bid->bid_state->bidder);
  winning_bid_info.render_url = winner->bid->ad_descriptor.url;
  winning_bid_info.ad_components = winner->bid->GetAdComponentUrls();
  // Need the bid from the bidder itself. If the bid was from a component
  // auction, then `top_bid_->bid` will be the bid from the component auction,
  // which the component seller worklet may have modified, and thus the wrong
  // bid. As a result, have to get the top bid from the component auction in
  // that case. `top_bid_->bid->auction->top_bid()` is the same as `top_bid_` if
  // the bid was from the top-level auction, and the original top bid from the
  // component auction, otherwise, so will always be the bid returned by the
  // winning bidder's generateBid() method.
  const InterestGroupAuction::Bid* bidder_bid =
      winner->bid->auction->top_bid()->bid.get();
  winning_bid_info.bid = bidder_bid->bid;
  winning_bid_info.bid_currency = bidder_bid->bid_currency;
  // We redact the bid currency if it's not a concrete currency from the config,
  // to avoid the bidder being able to leak ~14 bits of information if the
  // bidder currency configuration is not restrictive.
  absl::optional<blink::AdCurrency> config_currency = PerBuyerCurrency(
      bidder_bid->interest_group->owner, *bidder_bid->auction->config_);
  if (!config_currency.has_value()) {
    winning_bid_info.bid_currency = absl::nullopt;
  }

  winning_bid_info.ad_cost = bidder_bid->ad_cost;
  winning_bid_info.modeling_signals = bidder_bid->modeling_signals;
  winning_bid_info.bid_duration = winner->bid->bid_duration;
  winning_bid_info.bidding_signals_data_version =
      winner->bid->bidding_signals_data_version;
  if (winner->bid->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    winning_bid_info.ad_metadata =
        base::StringPrintf(R"({"render_url":"%s","metadata":%s})",
                           winner->bid->ad_descriptor.url.spec().c_str(),
                           winner->bid->bid_ad->metadata.value().c_str());
  } else {
    winning_bid_info.ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           winner->bid->ad_descriptor.url.spec().c_str());
  }

  InterestGroupAuctionReporter::SellerWinningBidInfo
      top_level_seller_winning_bid_info;
  top_level_seller_winning_bid_info.auction_config = config_;
  DCHECK(subresource_url_builder_);  // Must have been created by scoring.
  top_level_seller_winning_bid_info.subresource_url_builder =
      std::move(subresource_url_builder_);
  top_level_seller_winning_bid_info.bid = winner->bid->bid;
  top_level_seller_winning_bid_info.bid_in_seller_currency =
      winner->bid_in_seller_currency.value_or(0.0);
  top_level_seller_winning_bid_info.score = winner->score;
  top_level_seller_winning_bid_info.highest_scoring_other_bid =
      leader.highest_scoring_other_bid;
  top_level_seller_winning_bid_info
      .highest_scoring_other_bid_in_seller_currency =
      leader.highest_scoring_other_bid_in_seller_currency;
  top_level_seller_winning_bid_info.highest_scoring_other_bid_owner =
      leader.highest_scoring_other_bid_owner;
  top_level_seller_winning_bid_info.scoring_signals_data_version =
      leader.top_bid->scoring_signals_data_version;
  top_level_seller_winning_bid_info.trace_id = trace_id;

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
  if (winner->bid->auction != this) {
    InterestGroupAuction* component_auction = winner->bid->auction;
    component_seller_winning_bid_info.emplace();
    component_seller_winning_bid_info->auction_config =
        component_auction->config_;
    DCHECK(component_auction->subresource_url_builder_);
    component_seller_winning_bid_info->subresource_url_builder =
        std::move(component_auction->subresource_url_builder_);
    const LeaderInfo& component_leader = component_auction->leader_info();
    component_seller_winning_bid_info->bid = component_leader.top_bid->bid->bid;
    component_seller_winning_bid_info->bid_in_seller_currency =
        component_leader.top_bid->bid_in_seller_currency.value_or(0.0);
    component_seller_winning_bid_info->score = component_leader.top_bid->score;
    component_seller_winning_bid_info->highest_scoring_other_bid =
        component_leader.highest_scoring_other_bid;
    component_seller_winning_bid_info
        ->highest_scoring_other_bid_in_seller_currency =
        component_leader.highest_scoring_other_bid_in_seller_currency;
    component_seller_winning_bid_info->highest_scoring_other_bid_owner =
        component_leader.highest_scoring_other_bid_owner;
    component_seller_winning_bid_info->scoring_signals_data_version =
        component_leader.top_bid->scoring_signals_data_version;
    component_seller_winning_bid_info->trace_id = *component_auction->trace_id_;
    component_seller_winning_bid_info->component_auction_modified_bid_params =
        component_leader.top_bid->component_auction_modified_bid_params
            ->Clone();
  }

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
      debug_win_report_urls, debug_loss_report_urls);

  return std::make_unique<InterestGroupAuctionReporter>(
      interest_group_manager_, auction_worklet_manager_, attribution_manager,
      private_aggregation_manager,
      maybe_log_private_aggregation_web_features_callback_,
      std::move(auction_config), main_frame_origin, frame_origin,
      std::move(client_security_state), std::move(url_loader_factory),
      std::move(winning_bid_info), std::move(top_level_seller_winning_bid_info),
      std::move(component_seller_winning_bid_info),
      std::move(interest_groups_that_bid), std::move(debug_win_report_urls),
      std::move(debug_loss_report_urls), GetKAnonKeysToJoin(),
      TakeReservedPrivateAggregationRequests(),
      TakeNonReservedPrivateAggregationRequests());
}

void InterestGroupAuction::NotifyConfigPromisesResolved() {
  DCHECK(!config_promises_resolved_);
  DCHECK_EQ(0, config_->NumPromises());
  config_promises_resolved_ = true;
  for (const auto& buyer_helper : buyer_helpers_) {
    buyer_helper->NotifyConfigPromisesResolved();
  }

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& unscored_bid : unscored_bids_) {
    unscored_bid->wait_promises =
        now - unscored_bid->trace_wait_seller_deps_start;
  }

  ScoreQueuedBidsIfReady();
}

void InterestGroupAuction::NotifyComponentConfigPromisesResolved(uint32_t pos) {
  DCHECK(!parent_);  // Should not be called on a component.
  auto it = component_auctions_.find(pos);

  if (it == component_auctions_.end()) {
    // It's OK if the component auction isn't found; that means it got dropped
    // at database loading stage.
    return;
  }

  it->second->NotifyConfigPromisesResolved();
}

void InterestGroupAuction::ClosePipes() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  score_ad_receivers_.Clear();

  for (auto& buyer_helper : buyer_helpers_) {
    buyer_helper->ClosePipes();
  }
  seller_worklet_handle_.reset();

  // Close pipes for component auctions as well.
  for (auto& component_auction_info : component_auctions_) {
    component_auction_info.second->ClosePipes();
  }
}

size_t InterestGroupAuction::NumPotentialBidders() const {
  size_t num_interest_groups = 0;
  for (const auto& buyer_helper : buyer_helpers_) {
    num_interest_groups += buyer_helper->num_potential_bidders();
  }
  for (const auto& component_auction_info : component_auctions_) {
    num_interest_groups += component_auction_info.second->NumPotentialBidders();
  }
  return num_interest_groups;
}

void InterestGroupAuction::GetInterestGroupsThatBidAndReportBidCounts(
    blink::InterestGroupSet& interest_groups) const {
  if (!all_bids_scored_) {
    return;
  }

  for (auto& buyer_helper : buyer_helpers_) {
    buyer_helper->GetInterestGroupsThatBidAndReportBidCounts(interest_groups);
  }

  // Retrieve data from component auctions as well.
  for (const auto& component_auction_info : component_auctions_) {
    component_auction_info.second->GetInterestGroupsThatBidAndReportBidCounts(
        interest_groups);
  }
}

absl::optional<blink::AdSize> InterestGroupAuction::RequestedAdSize() const {
  return config_->non_shared_params.requested_size;
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
    case auction_worklet::mojom::RejectReason::kBelowKAnonThreshold:
      reject_reason_str = "below-kanon-threshold";
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
  if (!url.has_query()) {
    return url;
  }

  std::string query_string = url.query();
  base::ReplaceSubstringsAfterOffset(&query_string, 0, "${winningBid}",
                                     base::NumberToString(signals.winning_bid));
  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${winningBidCurrency}",
      blink::PrintableAdCurrency(signals.winning_bid_currency));

  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${madeWinningBid}",
      signals.made_winning_bid ? "true" : "false");
  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${highestScoringOtherBid}",
      base::NumberToString(signals.highest_scoring_other_bid));
  base::ReplaceSubstringsAfterOffset(
      &query_string, 0, "${highestScoringOtherBidCurrency}",
      blink::PrintableAdCurrency(signals.highest_scoring_other_bid_currency));
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
        &query_string, 0, "${topLevelWinningBidCurrency}",
        blink::PrintableAdCurrency(top_level_signals->winning_bid_currency));
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

bool InterestGroupAuction::ReportPaBuyersValueIfAllowed(
    const blink::InterestGroup& interest_group,
    blink::SellerCapabilities capability,
    blink::AuctionConfig::NonSharedParams::BuyerReportType buyer_report_type,
    int value) {
  if (!CanReportPaBuyersValue(interest_group, capability, config_->seller)) {
    return false;
  }

  absl::optional<absl::uint128> bucket_base =
      BucketBaseForReportPaBuyers(*config_, interest_group.owner);
  if (!bucket_base) {
    return false;
  }

  absl::optional<
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>
      report_buyers_config =
          ReportBuyersConfigForPaBuyers(buyer_report_type, *config_);
  if (!report_buyers_config) {
    return false;
  }

  // TODO(caraitto): Consider adding renderer and Mojo validation to ensure that
  // bucket sums can't be out of range, and scales can't be negative, infinite,
  // or NaN.
  PrivateAggregationRequests& destination_vector =
      private_aggregation_requests_reserved_[config_->seller];
  destination_vector.push_back(
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(
                  blink::mojom::AggregatableReportHistogramContribution::New(
                      *bucket_base + report_buyers_config->bucket,
                      base::saturated_cast<int32_t>(
                          std::max(0.0, value * report_buyers_config->scale)))),
          // TODO(caraitto): Consider allowing these to be set.
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New()));
  return true;
}

bool InterestGroupAuction::HasNonKAnonWinner() const {
  if (!final_auction_result_) {
    return false;
  }

  switch (*final_auction_result_) {
    // Bidding and scoring phase completed with no fatal error. We either
    // succeeded or only failed because we did not have a winner. If the only
    // reason we didn't have a winner was k-anonymity enforcement, there may
    // still be a non-k-anon winner.
    case AuctionResult::kSuccess:
    case AuctionResult::kNoBids:
    case AuctionResult::kAllBidsRejected:
      return top_non_kanon_enforced_bid() != nullptr;
    case AuctionResult::kAborted:
    case AuctionResult::kBadMojoMessage:
    case AuctionResult::kNoInterestGroups:
    case AuctionResult::kSellerWorkletLoadFailed:
    case AuctionResult::kSellerWorkletCrashed:
    case AuctionResult::kSellerRejected:
    case AuctionResult::kComponentLostAuction:
      return false;
  }
}

bool InterestGroupAuction::NonKAnonWinnerIsKAnon() const {
  return top_non_kanon_enforced_bid() &&
         top_non_kanon_enforced_bid()
                 ->bid->auction->top_non_kanon_enforced_bid()
                 ->bid->bid_role == Bid::BidRole::kBothKAnonModes;
}

SubresourceUrlBuilder* InterestGroupAuction::SubresourceUrlBuilderIfReady() {
  if (!subresource_url_builder_ &&
      !config_->direct_from_seller_signals.is_promise()) {
    subresource_url_builder_ = std::make_unique<SubresourceUrlBuilder>(
        config_->direct_from_seller_signals.value());
  }

  return subresource_url_builder_.get();
}

void InterestGroupAuction::
    TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
        std::vector<GURL>& debug_win_report_urls,
        std::vector<GURL>& debug_loss_report_urls) {
  if (!all_bids_scored_) {
    return;
  }

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
  const LeaderInfo& leader = leader_info();
  if (final_auction_result_ == AuctionResult::kSuccess &&
      leader.top_bid->bid->auction == this) {
    winner = leader.top_bid->bid->bid_state;
  }

  BidState* non_kanon_winner = nullptr;
  if (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kEnforce &&
      HasNonKAnonWinner() && !NonKAnonWinnerIsKAnon()) {
    non_kanon_winner = top_non_kanon_enforced_bid()->bid->bid_state;
  }

  // `signals` includes post auction signals from current auction.
  PostAuctionSignals signals;

  // `top_level_signals` includes post auction signals from top-level auction.
  // Will only will be used in debug report URLs of top-level seller and
  // component sellers.
  // For now, we're assuming top-level auctions to be first-price auction only
  // (not second-price auction) and it does not need highest_scoring_other_bid.
  absl::optional<PostAuctionSignals> top_level_signals;
  if (parent_) {
    top_level_signals.emplace();
  }

  if (!leader.top_bid) {
    DCHECK_EQ(leader.highest_scoring_other_bid, 0);
    DCHECK(!leader.highest_scoring_other_bid_owner.has_value());
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    const url::Origin& owner = buyer_helper->owner();
    if (leader.top_bid) {
      PostAuctionSignals::FillWinningBidInfo(
          owner, leader.top_bid->bid->interest_group->owner,
          leader.top_bid->bid->bid, leader.top_bid->bid_in_seller_currency,
          config_->non_shared_params.seller_currency, signals.made_winning_bid,
          signals.winning_bid, signals.winning_bid_currency);
    }

    PostAuctionSignals::FillRelevantHighestScoringOtherBidInfo(
        owner, leader.highest_scoring_other_bid_owner,
        leader.highest_scoring_other_bid,
        leader.highest_scoring_other_bid_in_seller_currency,
        config_->non_shared_params.seller_currency,
        signals.made_highest_scoring_other_bid,
        signals.highest_scoring_other_bid,
        signals.highest_scoring_other_bid_currency);

    if (parent_ && parent_->top_bid()) {
      PostAuctionSignals::FillWinningBidInfo(
          owner, parent_->top_bid()->bid->interest_group->owner,
          parent_->top_bid()->bid->bid,
          parent_->top_bid()->bid_in_seller_currency,
          parent_->config_->non_shared_params.seller_currency,
          top_level_signals->made_winning_bid, top_level_signals->winning_bid,
          top_level_signals->winning_bid_currency);
    }

    buyer_helper->TakeDebugReportUrls(winner, signals, top_level_signals,
                                      debug_win_report_urls,
                                      debug_loss_report_urls);

    std::map<url::Origin, PrivateAggregationRequests>
        private_aggregation_requests_reserved;
    std::map<std::string, PrivateAggregationRequests>
        private_aggregation_requests_non_reserved;
    buyer_helper->TakePrivateAggregationRequests(
        winner, non_kanon_winner, signals, top_level_signals,
        private_aggregation_requests_reserved,
        private_aggregation_requests_non_reserved);

    for (auto& [origin, requests] : private_aggregation_requests_reserved) {
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_reserved_[origin];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
    for (auto& [event_type, requests] :
         private_aggregation_requests_non_reserved) {
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_non_reserved_[event_type];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction_info : component_auctions_) {
    component_auction_info.second
        ->TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
            debug_win_report_urls, debug_loss_report_urls);
  }
}

std::map<url::Origin, InterestGroupAuction::PrivateAggregationRequests>
InterestGroupAuction::TakeReservedPrivateAggregationRequests() {
  for (auto& component_auction_info : component_auctions_) {
    std::map<url::Origin, PrivateAggregationRequests> requests_map =
        component_auction_info.second->TakeReservedPrivateAggregationRequests();
    for (auto& [origin, requests] : requests_map) {
      DCHECK(!requests.empty());
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_reserved_[origin];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
  return std::move(private_aggregation_requests_reserved_);
}

std::map<std::string, InterestGroupAuction::PrivateAggregationRequests>
InterestGroupAuction::TakeNonReservedPrivateAggregationRequests() {
  for (auto& component_auction_info : component_auctions_) {
    std::map<std::string, PrivateAggregationRequests> requests_map =
        component_auction_info.second
            ->TakeNonReservedPrivateAggregationRequests();
    for (auto& [event_type, requests] : requests_map) {
      DCHECK(!requests.empty());
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_non_reserved_[event_type];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
  return std::move(private_aggregation_requests_non_reserved_);
}

std::vector<std::string> InterestGroupAuction::TakeErrors() {
  for (auto& component_auction_info : component_auctions_) {
    std::vector<std::string> errors =
        component_auction_info.second->TakeErrors();
    errors_.insert(errors_.begin(), errors.begin(), errors.end());
  }
  return std::move(errors_);
}

void InterestGroupAuction::TakePostAuctionUpdateOwners(
    std::vector<url::Origin>& owners) {
  for (const url::Origin& owner : post_auction_update_owners_) {
    owners.emplace_back(std::move(owner));
  }

  for (auto& component_auction_info : component_auctions_) {
    component_auction_info.second->TakePostAuctionUpdateOwners(owners);
  }
}

bool InterestGroupAuction::ReportInterestGroupCount(
    const blink::InterestGroup& interest_group,
    size_t count) {
  return ReportPaBuyersValueIfAllowed(
      interest_group, blink::SellerCapabilities::kInterestGroupCounts,
      blink::AuctionConfig::NonSharedParams::BuyerReportType::
          kInterestGroupCount,
      count);
}

bool InterestGroupAuction::ReportBidCount(
    const blink::InterestGroup& interest_group,
    size_t count) {
  return ReportPaBuyersValueIfAllowed(
      interest_group, blink::SellerCapabilities::kInterestGroupCounts,
      blink::AuctionConfig::NonSharedParams::BuyerReportType::kBidCount, count);
}

void InterestGroupAuction::ReportTrustedSignalsFetchLatency(
    const blink::InterestGroup& interest_group,
    base::TimeDelta trusted_signals_fetch_latency) {
  ReportPaBuyersValueIfAllowed(interest_group,
                               blink::SellerCapabilities::kLatencyStats,
                               blink::AuctionConfig::NonSharedParams::
                                   BuyerReportType::kTotalSignalsFetchLatency,
                               trusted_signals_fetch_latency.InMilliseconds());
}

void InterestGroupAuction::ReportBiddingLatency(
    const blink::InterestGroup& interest_group,
    base::TimeDelta bidding_latency) {
  ReportPaBuyersValueIfAllowed(interest_group,
                               blink::SellerCapabilities::kLatencyStats,
                               blink::AuctionConfig::NonSharedParams::
                                   BuyerReportType::kTotalGenerateBidLatency,
                               bidding_latency.InMilliseconds());
}

base::flat_set<std::string> InterestGroupAuction::GetKAnonKeysToJoin() const {
  if (!HasNonKAnonWinner()) {
    return {};
  }

  // If the k-anon winner is the same as the non-k-anon winner then just include
  // it once.
  std::vector<const ScoredBid*> bids_to_include = {
      top_non_kanon_enforced_bid()};
  if (!NonKAnonWinnerIsKAnon()) {
    bids_to_include.push_back(top_kanon_enforced_bid());
  }

  // Add all the KAnon keys for the winning k-anon and non-k-anon bids.
  std::vector<std::string> k_anon_keys_to_join;
  for (const ScoredBid* scored_bid : bids_to_include) {
    if (!scored_bid) {
      continue;
    }
    DCHECK(scored_bid->bid);
    const blink::InterestGroup& interest_group =
        *scored_bid->bid->interest_group;
    k_anon_keys_to_join.push_back(blink::KAnonKeyForAdBid(
        interest_group, scored_bid->bid->bid_ad->render_url));
    k_anon_keys_to_join.push_back(blink::KAnonKeyForAdNameReporting(
        interest_group, *scored_bid->bid->bid_ad));
    for (const blink::AdDescriptor& ad_component_descriptor :
         scored_bid->bid->ad_component_descriptors) {
      k_anon_keys_to_join.push_back(
          blink::KAnonKeyForAdComponentBid(ad_component_descriptor));
    }
  }
  return base::flat_set<std::string>(std::move(k_anon_keys_to_join));
}

void InterestGroupAuction::MaybeLogPrivateAggregationWebFeatures(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        private_aggregation_requests) {
  DCHECK(maybe_log_private_aggregation_web_features_callback_);
  maybe_log_private_aggregation_web_features_callback_.Run(
      private_aggregation_requests);
}

const InterestGroupAuction::LeaderInfo& InterestGroupAuction::leader_info()
    const {
  if (kanon_mode_ == auction_worklet::mojom::KAnonymityBidMode::kEnforce) {
    return kanon_enforced_auction_leader_;
  } else {
    return non_kanon_enforced_auction_leader_;
  }
}

InterestGroupAuction::ScoredBid*
InterestGroupAuction::top_kanon_enforced_bid() {
  return kanon_enforced_auction_leader_.top_bid.get();
}
const InterestGroupAuction::ScoredBid*
InterestGroupAuction::top_kanon_enforced_bid() const {
  return kanon_enforced_auction_leader_.top_bid.get();
}

InterestGroupAuction::ScoredBid*
InterestGroupAuction::top_non_kanon_enforced_bid() {
  return non_kanon_enforced_auction_leader_.top_bid.get();
}

const InterestGroupAuction::ScoredBid*
InterestGroupAuction::top_non_kanon_enforced_bid() const {
  return non_kanon_enforced_auction_leader_.top_bid.get();
}

absl::optional<uint16_t> InterestGroupAuction::GetBuyerExperimentId(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  auto it = config.per_buyer_experiment_group_ids.find(buyer);
  if (it != config.per_buyer_experiment_group_ids.end()) {
    return it->second;
  }
  return config.all_buyer_experiment_group_id;
}

absl::optional<std::string> InterestGroupAuction::GetPerBuyerSignals(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  const auto& auction_config_per_buyer_signals =
      config.non_shared_params.per_buyer_signals;
  DCHECK(!auction_config_per_buyer_signals.is_promise());
  if (auction_config_per_buyer_signals.value().has_value()) {
    auto it = auction_config_per_buyer_signals.value()->find(buyer);
    if (it != auction_config_per_buyer_signals.value()->end()) {
      return it->second;
    }
  }
  return absl::nullopt;
}

absl::optional<GURL> InterestGroupAuction::GetDirectFromSellerAuctionSignals(
    const SubresourceUrlBuilder* subresource_url_builder) {
  if (subresource_url_builder && subresource_url_builder->auction_signals()) {
    return subresource_url_builder->auction_signals()->subresource_url;
  }
  return absl::nullopt;
}

absl::optional<GURL> InterestGroupAuction::GetDirectFromSellerPerBuyerSignals(
    const SubresourceUrlBuilder* subresource_url_builder,
    const url::Origin& owner) {
  if (!subresource_url_builder) {
    return absl::nullopt;
  }

  auto it = subresource_url_builder->per_buyer_signals().find(owner);
  if (it == subresource_url_builder->per_buyer_signals().end()) {
    return absl::nullopt;
  }
  return it->second.subresource_url;
}

absl::optional<GURL> InterestGroupAuction::GetDirectFromSellerSellerSignals(
    const SubresourceUrlBuilder* subresource_url_builder) {
  if (subresource_url_builder && subresource_url_builder->seller_signals()) {
    return subresource_url_builder->seller_signals()->subresource_url;
  }
  return absl::nullopt;
}

InterestGroupAuction::LeaderInfo::LeaderInfo() = default;
InterestGroupAuction::LeaderInfo::~LeaderInfo() = default;

void InterestGroupAuction::OnInterestGroupRead(
    std::vector<StorageInterestGroup> interest_groups) {
  ++num_owners_loaded_;
  if (interest_groups.empty()) {
    OnOneLoadCompleted();
    return;
  }
  for (const StorageInterestGroup& group : interest_groups) {
    if (ReportInterestGroupCount(group.interest_group,
                                 interest_groups.size())) {
      break;
    }
  }
  post_auction_update_owners_.push_back(
      interest_groups[0].interest_group.owner);
  for (const auto& bidder : interest_groups) {
    // Report freshness metrics.
    if (bidder.interest_group.update_url.has_value()) {
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

  // Ignore interest groups that don't provide the requested seller
  // capabilities.
  base::EraseIf(interest_groups, [this](const StorageInterestGroup& bidder) {
    return !GroupSatisfiesAllCapabilities(
        bidder.interest_group,
        config_->non_shared_params.required_seller_capabilities,
        config_->seller);
  });

  // If there are no interest groups left, nothing else to do.
  if (interest_groups.empty()) {
    OnOneLoadCompleted();
    return;
  }

  ++num_owners_with_interest_groups_;
  auction_metrics_recorder_->ReportBuyer(
      interest_groups[0].interest_group.owner);

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
    AuctionMap::iterator component_auction,
    bool success) {
  num_owners_loaded_ += component_auction->second->num_owners_loaded_;
  num_owners_with_interest_groups_ +=
      component_auction->second->num_owners_with_interest_groups_;

  // Erase component auctions that failed to load anything, so they won't be
  // invoked in the generate bid phase. This is not a problem in the reporting
  // phase, as the top-level auction knows which component auction, if any, won.
  if (!success) {
    component_auctions_.erase(component_auction);
  }
  OnOneLoadCompleted();
}

void InterestGroupAuction::OnOneLoadCompleted() {
  DCHECK_GT(num_pending_loads_, 0u);
  --num_pending_loads_;

  // Wait for more buyers to be loaded, if there are still some pending.
  if (num_pending_loads_ > 0) {
    return;
  }

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
      if (num_interest_groups > 0) {
        ++num_sellers_with_bidders;
      }

      UMA_HISTOGRAM_COUNTS_1000("Ads.InterestGroup.Auction.NumInterestGroups",
                                num_interest_groups);
      auction_metrics_recorder_->SetNumInterestGroups(num_interest_groups);

      UMA_HISTOGRAM_COUNTS_100(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          num_owners_with_interest_groups_);
      auction_metrics_recorder_->SetNumOwnersWithInterestGroups(
          num_owners_with_interest_groups_);

      UMA_HISTOGRAM_COUNTS_100(
          "Ads.InterestGroup.Auction.NumSellersWithBidders",
          num_sellers_with_bidders);
      auction_metrics_recorder_->SetNumSellersWithBidders(
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

  if (!parent_) {
    auction_metrics_recorder_->OnLoadInterestGroupPhaseComplete();
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "load_groups_phase", *trace_id_);
  if (auction_result == AuctionResult::kNoInterestGroups) {
    UMA_HISTOGRAM_TIMES("Ads.InterestGroup.Auction.LoadNoGroupsTime",
                        base::TimeTicks::Now() - creation_time_);
  } else {
    UMA_HISTOGRAM_TIMES("Ads.InterestGroup.Auction.LoadGroupsTime",
                        base::TimeTicks::Now() - creation_time_);
  }

  // `final_auction_result_` should only be set to kSuccess when the entire
  // auction is complete.
  //
  // TODO(https://crbug.com/1394777): We should probably add new states for
  // whether the result was used, reports sent, etc, so either the
  // InterestGroupAuction or the InterestGroupAuctionReporter logs a single
  // result. Alternatively, we could add a separate histogram just for the
  // reporter stuff, which should have exactly as many entries as the historam
  // `final_auction_result_` is logged to.
  bool success = auction_result == AuctionResult::kSuccess;
  if (!success) {
    final_auction_result_ = auction_result;
  }
  std::move(load_interest_groups_phase_callback_).Run(success);
}

void InterestGroupAuction::OnComponentSellerWorkletReceived() {
  DCHECK_GT(pending_component_seller_worklet_requests_, 0u);
  --pending_component_seller_worklet_requests_;
  if (pending_component_seller_worklet_requests_ == 0) {
    RequestSellerWorklet();
  }
}

void InterestGroupAuction::RequestSellerWorklet() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "request_seller_worklet",
                                    *trace_id_);
  auction_worklet_manager_->RequestSellerWorklet(
      config_->decision_logic_url, config_->trusted_scoring_signals_url,
      config_->seller_experiment_group_id,
      base::BindOnce(&InterestGroupAuction::OnSellerWorkletReceived,
                     base::Unretained(this)),
      base::BindOnce(&InterestGroupAuction::OnSellerWorkletFatalError,
                     base::Unretained(this)),
      seller_worklet_handle_);
}

void InterestGroupAuction::OnSellerWorkletReceived() {
  DCHECK(!seller_worklet_received_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "request_seller_worklet",
                                  *trace_id_);

  if (on_seller_receiver_callback_) {
    std::move(on_seller_receiver_callback_).Run();
  }

  seller_worklet_received_ = true;

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& unscored_bid : unscored_bids_) {
    unscored_bid->wait_worklet =
        now - unscored_bid->trace_wait_seller_deps_start;
  }

  ScoreQueuedBidsIfReady();
}

void InterestGroupAuction::ScoreQueuedBidsIfReady() {
  if (!ReadyToScoreBids() || unscored_bids_.empty()) {
    return;
  }

  auto unscored_bids = std::move(unscored_bids_);
  for (auto& unscored_bid : unscored_bids) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "fledge", "wait_for_seller_deps", unscored_bid->TraceId(), "data",
        [&](perfetto::TracedValue trace_context) {
          auto dict = std::move(trace_context).WriteDictionary();
          if (!unscored_bid->wait_worklet.is_zero()) {
            dict.Add("wait_worklet_ms",
                     unscored_bid->wait_worklet.InMillisecondsF());
          }
          if (!unscored_bid->wait_promises.is_zero()) {
            dict.Add("wait_promises_ms",
                     unscored_bid->wait_promises.InMillisecondsF());
          }
        });
    ScoreBidIfReady(std::move(unscored_bid));
  }

  // If no further bids are outstanding, now is the time to send a coalesced
  // request for all the trusted seller signals (if some still are pending,
  // OnBidSourceDone() will take care of it).
  if (outstanding_bid_sources_ == 0) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
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
  auction_metrics_recorder_->RecordComponentAuctionLatency(
      base::TimeTicks::Now() - bidding_and_scoring_phase_start_time_);

  // TODO(morlovich): Can try to consolidate these as kBothKAnonModes when
  // possible.
  ScoredBid* non_kanon_enforced_bid =
      component_auction->top_non_kanon_enforced_bid();
  if (non_kanon_enforced_bid) {
    // There is no need to potentially turn this into an k-anon enforced bid
    // since that already happened when running the component auction.
    ScoreBidIfReady(CreateBidFromComponentAuctionWinner(
        non_kanon_enforced_bid, Bid::BidRole::kUnenforcedKAnon));
  }

  ScoredBid* kanon_bid = component_auction->top_kanon_enforced_bid();
  if (kanon_bid) {
    ScoreBidIfReady(CreateBidFromComponentAuctionWinner(
        kanon_bid, Bid::BidRole::kEnforcedKAnon));
  }

  OnBidSourceDone();
}

// static
std::unique_ptr<InterestGroupAuction::Bid>
InterestGroupAuction::CreateBidFromComponentAuctionWinner(
    const ScoredBid* scored_bid,
    Bid::BidRole bid_role) {
  // Create a copy of component Auction's bid, replacing values as necessary.
  const Bid* component_bid = scored_bid->bid.get();
  const auto* modified_bid_params =
      scored_bid->component_auction_modified_bid_params.get();
  DCHECK(modified_bid_params);

  // Create a new event for the bid, since the component auction's event for
  // it ended after the component auction scored the bid.
  if (bid_role == Bid::BidRole::kEnforcedKAnon) {
    if (!component_bid->bid_state->trace_id_for_kanon_scoring.has_value()) {
      component_bid->bid_state->BeginTracingKAnonScoring();
    }
  } else {
    if (!component_bid->bid_state->trace_id.has_value()) {
      component_bid->bid_state->BeginTracing();
    }
  }

  return std::make_unique<Bid>(
      bid_role, modified_bid_params->ad,
      modified_bid_params->has_bid ? modified_bid_params->bid
                                   : component_bid->bid,
      modified_bid_params->has_bid ? modified_bid_params->bid_currency
                                   : component_bid->bid_currency,
      component_bid->ad_cost, component_bid->ad_descriptor,
      component_bid->ad_component_descriptors, component_bid->modeling_signals,
      component_bid->bid_duration, component_bid->bidding_signals_data_version,
      component_bid->bid_ad, component_bid->bid_state, component_bid->auction);
}

void InterestGroupAuction::OnBidSourceDone() {
  --outstanding_bid_sources_;

  // If we issued the final set of bids to a seller worklet, tell it to send any
  // pending scoring signals request to complete the auction more quickly.
  if (outstanding_bid_sources_ == 0 && ReadyToScoreBids()) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
  }

  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::ScoreBidIfReady(std::unique_ptr<Bid> bid) {
  DCHECK(bid);
  DCHECK(bid->bid_state->made_bid);

  any_bid_made_ = true;

  // If seller worklet hasn't been received yet, or configuration is still
  // waiting on some promises, wait till everything is ready.
  // TODO(morlovich): Tracing doesn't reflect config wait here.
  uint64_t bid_trace_id = bid->TraceId();
  if (!ReadyToScoreBids()) {
    bid->trace_wait_seller_deps_start = base::TimeTicks::Now();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_for_seller_deps",
                                      bid_trace_id);
    unscored_bids_.emplace_back(std::move(bid));
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("fledge", ScoreAdTraceEventName(*bid),
                                    bid_trace_id, "decision_logic_url",
                                    config_->decision_logic_url);

  ++bids_being_scored_;
  Bid* bid_raw = bid.get();

  mojo::PendingRemote<auction_worklet::mojom::ScoreAdClient> score_ad_remote;
  score_ad_receivers_.Add(
      this, score_ad_remote.InitWithNewPipeAndPassReceiver(), std::move(bid));
  DCHECK_EQ(0, config_->NumPromises());
  SubresourceUrlBuilder* url_builder = SubresourceUrlBuilderIfReady();
  DCHECK(url_builder);  // Should be ready by now.
  seller_worklet_handle_->AuthorizeSubresourceUrls(*url_builder);
  seller_worklet_handle_->GetSellerWorklet()->ScoreAd(
      bid_raw->ad_metadata, bid_raw->bid, bid_raw->bid_currency,
      config_->non_shared_params, GetDirectFromSellerSellerSignals(url_builder),
      GetDirectFromSellerAuctionSignals(url_builder),
      GetOtherSellerParam(*bid_raw),
      parent_ ? PerBuyerCurrency(config_->seller, *parent_->config_)
              : absl::nullopt,
      bid_raw->interest_group->owner, bid_raw->ad_descriptor.url,
      bid_raw->GetAdComponentUrls(), bid_raw->bid_duration.InMilliseconds(),
      SellerTimeout(), bid_trace_id, std::move(score_ad_remote));
}

bool InterestGroupAuction::ValidateScoreBidCompleteResult(
    double score,
    auction_worklet::mojom::ComponentAuctionModifiedBidParams*
        component_auction_modified_bid_params,
    absl::optional<double> bid_in_seller_currency,
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
    // If a component seller modified the bid, the new bid must also be valid,
    // as should its currency.
    if (component_auction_modified_bid_params &&
        component_auction_modified_bid_params->has_bid) {
      if (!IsValidBid(component_auction_modified_bid_params->bid)) {
        score_ad_receivers_.ReportBadMessage(
            "Invalid component_auction_modified_bid_params bid");
        return false;
      }

      if (!blink::VerifyAdCurrencyCode(
              config_->non_shared_params.seller_currency,
              component_auction_modified_bid_params->bid_currency) ||
          !blink::VerifyAdCurrencyCode(
              PerBuyerCurrency(config_->seller, *parent_->config_),
              component_auction_modified_bid_params->bid_currency)) {
        score_ad_receivers_.ReportBadMessage(
            "Invalid component_auction_modified_bid_params bid_currency");
        return false;
      }
    }
  }
  if (bid_in_seller_currency.has_value() &&
      (!IsValidBid(*bid_in_seller_currency) ||
       !config_->non_shared_params.seller_currency.has_value())) {
    score_ad_receivers_.ReportBadMessage("Invalid bid_in_seller_currency");
    return false;
  }
  return true;
}

void InterestGroupAuction::OnScoreAdComplete(
    double score,
    auction_worklet::mojom::RejectReason reject_reason,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    absl::optional<double> bid_in_seller_currency,
    absl::optional<uint32_t> scoring_signals_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    const std::vector<std::string>& errors) {
  DCHECK_GT(bids_being_scored_, 0);

  if (!ValidateScoreBidCompleteResult(
          score, component_auction_modified_bid_params.get(),
          bid_in_seller_currency, debug_loss_report_url,
          debug_win_report_url)) {
    OnBiddingAndScoringComplete(AuctionResult::kBadMojoMessage);
    return;
  }

  std::unique_ptr<Bid> bid = std::move(score_ad_receivers_.current_context());
  score_ad_receivers_.Remove(score_ad_receivers_.current_receiver());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", ScoreAdTraceEventName(*bid),
                                  bid->TraceId());
  if (bid->bid_role == Bid::BidRole::kEnforcedKAnon) {
    bid->bid_state->EndTracingKAnonScoring();
  } else {
    bid->bid_state->EndTracing();
  }

  --bids_being_scored_;

  // Reporting and the like should be done only for things that go into the
  // run that produces the result of runAdAuction().
  if (IsBidRoleUsedForWinner(kanon_mode_, bid->bid_role)) {
    // The mojom API declaration should ensure none of these are null.
    DCHECK(base::ranges::none_of(
        pa_requests,
        [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
               request_ptr) { return request_ptr.is_null(); }));
    MaybeLogPrivateAggregationWebFeatures(pa_requests);
    if (!pa_requests.empty()) {
      DCHECK(config_);
      PrivateAggregationRequests& pa_requests_for_seller =
          bid->bid_state->private_aggregation_requests[std::make_pair(
              config_->seller, !parent_)];
      for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
           pa_requests) {
        // A for-event private aggregation request with non-reserved event type
        // from scoreAd() should be ignored and not reported.
        if (request->contribution->is_for_event_contribution() &&
            !base::StartsWith(
                request->contribution->get_for_event_contribution()->event_type,
                "reserved.")) {
          continue;
        }
        pa_requests_for_seller.emplace_back(std::move(request));
      }
    }

    // Use separate fields for component and top-level seller reports, so both
    // can send debug reports.
    if (bid->auction == this) {
      bid->bid_state->seller_debug_loss_report_url =
          std::move(debug_loss_report_url);
      bid->bid_state->seller_debug_win_report_url =
          std::move(debug_win_report_url);
      // Ignores reject reason if score > 0.
      // TODO(qingxinwu): Set bid_state->reject_reason to nullopt instead of
      // kNotAvailable when score > 0.
      if (score <= 0) {
        bid->bid_state->reject_reason = reject_reason;
      }
    } else {
      bid->bid_state->top_level_seller_debug_loss_report_url =
          std::move(debug_loss_report_url);
      bid->bid_state->top_level_seller_debug_win_report_url =
          std::move(debug_win_report_url);
    }
  }

  // Debug output to developers should include all of errors.
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // A score <= 0 means the seller rejected the bid.
  if (score > 0) {
    switch (bid->bid_role) {
      case Bid::BidRole::kUnenforcedKAnon:
        UpdateAuctionLeaders(std::move(bid), score,
                             std::move(component_auction_modified_bid_params),
                             bid_in_seller_currency,
                             scoring_signals_data_version,
                             non_kanon_enforced_auction_leader_);
        break;
      case Bid::BidRole::kEnforcedKAnon:
        UpdateAuctionLeaders(std::move(bid), score,
                             std::move(component_auction_modified_bid_params),
                             bid_in_seller_currency,
                             scoring_signals_data_version,
                             kanon_enforced_auction_leader_);
        break;
      case Bid::BidRole::kBothKAnonModes: {
        auto bid_copy = std::make_unique<Bid>(*bid);
        auto modified_bid_params_copy =
            component_auction_modified_bid_params
                ? component_auction_modified_bid_params->Clone()
                : auction_worklet::mojom::
                      ComponentAuctionModifiedBidParamsPtr();
        UpdateAuctionLeaders(std::move(bid), score,
                             std::move(component_auction_modified_bid_params),
                             bid_in_seller_currency,
                             scoring_signals_data_version,
                             non_kanon_enforced_auction_leader_);
        UpdateAuctionLeaders(
            std::move(bid_copy), score, std::move(modified_bid_params_copy),
            bid_in_seller_currency, scoring_signals_data_version,
            kanon_enforced_auction_leader_);
      }
    }
  }

  // Need to delete `bid` because OnBiddingAndScoringComplete() may delete
  // this, which leaves danging pointers on the stack. While this is safe to
  // do (nothing has access to `bid` to dereference them), it makes the
  // dangling pointer tooling sad.
  bid.reset();
  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::UpdateAuctionLeaders(
    std::unique_ptr<Bid> bid,
    double score,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    absl::optional<double> bid_in_seller_currency,
    absl::optional<uint32_t> scoring_signals_data_version,
    LeaderInfo& leader_info) {
  bool is_top_bid = false;
  const url::Origin& owner = bid->interest_group->owner;

  if (!leader_info.top_bid || score > leader_info.top_bid->score) {
    // If there's no previous top bidder, or the bidder has the highest score,
    // need to replace the previous top bidder.
    is_top_bid = true;
    if (leader_info.top_bid) {
      OnNewHighestScoringOtherBid(
          leader_info.top_bid->score, leader_info.top_bid->bid->bid,
          leader_info.top_bid->bid_in_seller_currency,
          &leader_info.top_bid->bid->interest_group->owner, leader_info);
    }
    leader_info.num_top_bids = 1;
    leader_info.at_most_one_top_bid_owner = true;
  } else if (score == leader_info.top_bid->score) {
    // If there's a tie, replace the top-bidder with 1-in-`num_top_bids_`
    // chance. This is the select random value from a stream with fixed
    // storage problem.
    ++leader_info.num_top_bids;
    if (1 == base::RandInt(1, leader_info.num_top_bids)) {
      is_top_bid = true;
    }
    if (owner != leader_info.top_bid->bid->interest_group->owner) {
      leader_info.at_most_one_top_bid_owner = false;
    }
    // If the top bid is being replaced, need to add the old top bid as a second
    // highest bid. Otherwise, need to add the current bid as a second highest
    // bid.
    double new_highest_scoring_other_bid =
        is_top_bid ? leader_info.top_bid->bid->bid : bid->bid;
    absl::optional<double> new_highest_scoring_other_bid_in_seller_currency =
        is_top_bid ? leader_info.top_bid->bid_in_seller_currency
                   : bid_in_seller_currency;
    OnNewHighestScoringOtherBid(
        score, new_highest_scoring_other_bid,
        new_highest_scoring_other_bid_in_seller_currency,
        leader_info.at_most_one_top_bid_owner ? &bid->interest_group->owner
                                              : nullptr,
        leader_info);
  } else if (score >= leader_info.second_highest_score) {
    // Also use this bid (the most recent one) as highest scoring other bid if
    // there's a tie for second highest score.
    OnNewHighestScoringOtherBid(score, bid->bid, bid_in_seller_currency, &owner,
                                leader_info);
  }

  if (is_top_bid) {
    leader_info.top_bid = std::make_unique<ScoredBid>(
        score, std::move(scoring_signals_data_version), std::move(bid),
        std::move(bid_in_seller_currency),
        std::move(component_auction_modified_bid_params));
  }
}

void InterestGroupAuction::OnNewHighestScoringOtherBid(
    double score,
    double bid_value,
    absl::optional<double> bid_in_seller_currency,
    const url::Origin* owner,
    LeaderInfo& leader_info) {
  // Current (the most recent) bid becomes highest scoring other bid.
  if (score > leader_info.second_highest_score) {
    leader_info.highest_scoring_other_bid = bid_value;
    leader_info.highest_scoring_other_bid_in_seller_currency =
        bid_in_seller_currency;
    leader_info.num_second_highest_bids = 1;
    // Owner may be false if this is one of the bids tied for first place.
    if (!owner) {
      leader_info.highest_scoring_other_bid_owner.reset();
    } else {
      leader_info.highest_scoring_other_bid_owner = *owner;
    }
    leader_info.second_highest_score = score;
    return;
  }

  DCHECK_EQ(score, leader_info.second_highest_score);
  if (!owner || *owner != leader_info.highest_scoring_other_bid_owner) {
    leader_info.highest_scoring_other_bid_owner.reset();
  }
  ++leader_info.num_second_highest_bids;
  // In case of a tie, randomly pick one. This is the select random value from a
  // stream with fixed storage problem.
  if (1 == base::RandInt(1, leader_info.num_second_highest_bids)) {
    leader_info.highest_scoring_other_bid = bid_value;
    leader_info.highest_scoring_other_bid_in_seller_currency =
        bid_in_seller_currency;
  }
}

absl::optional<base::TimeDelta> InterestGroupAuction::SellerTimeout() {
  if (config_->non_shared_params.seller_timeout.has_value()) {
    return std::min(config_->non_shared_params.seller_timeout.value(),
                    kMaxPerBuyerTimeout);
  }
  return absl::nullopt;
}

void InterestGroupAuction::MaybeCompleteBiddingAndScoringPhase() {
  if (!AllBidsScored()) {
    return;
  }

  all_bids_scored_ = true;

  // If there's no winning bid, fail with kAllBidsRejected if there were any
  // bids. Otherwise, fail with kNoBids.
  if (!top_bid()) {
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
                                  *trace_id_);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // If this is a component auction, have to unload the seller worklet handle to
  // avoid deadlock. Otherwise, loading the top-level seller worklet may be
  // blocked by component seller worklets taking up all the quota.
  if (parent_) {
    seller_worklet_handle_.reset();
  }

  // If the seller loaded callback hasn't been invoked yet, call it now. This is
  // needed in the case the phase ended without receiving the seller worklet
  // (e.g., in the case no bidder worklet bids).
  if (on_seller_receiver_callback_) {
    std::move(on_seller_receiver_callback_).Run();
  }

  bool success = auction_result == AuctionResult::kSuccess;
  if (!success) {
    // Close all pipes, to prevent any pending callbacks from being invoked if
    // this phase is being completed due to a fatal error, like the seller
    // worklet failing to load.
    ClosePipes();

    // `final_auction_result_` should only be set to kSuccess when the entire
    // auction is complete.
    final_auction_result_ = auction_result;
  } else {
    // If this is the top-level auction, mark it as a success (and the winning
    // component auction as well, if there is one). Component auction status
    // depend on whether or not they won the top-level auction, so if this is a
    // component auciton, leave status as-is.
    if (!parent_) {
      final_auction_result_ = AuctionResult::kSuccess;
      // If there's a winning bid, set its auction result as well. If the
      // winning bid came from a component auction, this will set that component
      // auction's result as well. This is needed for auction result accessors.
      //
      // TODO(https://crbug.com/1394777): This is currently needed to correctly
      // retrieve reporting information from the nested auction. Is there a
      // cleaner way to do this?
      if (top_bid()) {
        top_bid()->bid->auction->final_auction_result_ =
            AuctionResult::kSuccess;
      }
    }
  }

  // If this is a top-level auction with component auction, update final state
  // of all successfully completed component auctions with bids that did not win
  // to reflect a loss.
  for (auto& component_auction_info : component_auctions_) {
    InterestGroupAuction* component_auction =
        component_auction_info.second.get();
    // Leave the state of the winning component auction alone, if the winning
    // bid is from a component auction.
    ScoredBid* winner = top_bid();
    if (winner && winner->bid->auction == component_auction) {
      continue;
    }
    if (component_auction->final_auction_result_) {
      continue;
    }
    component_auction->final_auction_result_ =
        AuctionResult::kComponentLostAuction;
  }

  std::move(bidding_and_scoring_phase_callback_).Run(success);
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

AuctionWorkletManager::WorkletKey InterestGroupAuction::BidderWorkletKey(
    BidState& bid_state) {
  DCHECK(!bid_state.worklet_handle);

  const blink::InterestGroup& interest_group = bid_state.bidder->interest_group;

  absl::optional<uint16_t> experiment_group_id =
      GetBuyerExperimentId(*config_, interest_group.owner);

  return AuctionWorkletManager::BidderWorkletKey(
      interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url, experiment_group_id);
}

}  // namespace content
