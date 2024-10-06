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
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
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
#include "base/token.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/additional_bid_result.h"
#include "content/browser/interest_group/additional_bids_util.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_nonce_manager.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/bidding_and_auction_response.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/browser/interest_group/header_direct_from_seller_signals.h"
#include "content/browser/interest_group/interest_group_auction_reporter.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/interest_group/interest_group_priority_util.h"
#include "content/browser/interest_group/interest_group_real_time_report_util.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/features.h"
#include "content/public/browser/auction_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "content/services/auction_worklet/public/cpp/real_time_reporting.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_response.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/devtools_serialization.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// The BiddingAndAuctionEncryptionMediaType feature controls the format we use
// for the request to the Bidding and Auction Service. When enabled we add an
// extra byte to the request and use the new media types instead of the
// defaults from libquiche.
CONTENT_EXPORT BASE_FEATURE(kBiddingAndAuctionEncryptionMediaType,
                            "BiddingAndAuctionEncryptionMediaType",
                            base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

constexpr base::TimeDelta kMaxPerBuyerTimeout = base::Milliseconds(500);

// For group freshness metrics.
constexpr base::TimeDelta kGroupFreshnessMin = base::Minutes(1);
constexpr base::TimeDelta kGroupFreshnessMax = base::Days(30);
constexpr int kGroupFreshnessBuckets = 100;

// For updateIfOlderThan metrics.
constexpr base::TimeDelta kMinUpdateIfOlderThanHistogram = base::Minutes(-10);
constexpr base::TimeDelta kMaxUpdateIfOlderThanHistogram = base::Hours(30);
constexpr size_t kUpdateIfOlderThanBuckets = 100;

constexpr char kInvalidServerResponseReasonUMAName[] =
    "Ads.InterestGroup.ServerAuction.InvalidServerResponseReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InvalidServerResponseReason {
  kUnknown = 0,
  kNotWitnessed = 1,
  kUnknownRequestId = 2,
  kSellerMismatch = 3,
  kDecryptionFailure = 4,
  kUnframingFailure = 5,
  kDecompressFailure = 6,
  kCBORParseFailure = 7,
  kUnexpectedResponseStructure = 8,
  kNotMultilevel = 9,
  kTopLevelSellerMismatch = 10,
  kNotSingleLevel = 11,
  kInvalidBid = 12,
  kWinningGroupNotBidder = 13,
  kMissingWinningGroup = 14,
  kMissingWinningGroupBidURL = 15,
  kConstructBidFailure = 16,
  kBadCurrency = 17,
  kDecoderShutdown = 18,
  kMaxValue = kDecoderShutdown,
};

std::string DirectFromSellerSignalsHeaderAdSlotNoMatchError(
    const std::string& ad_slot) {
  return base::StringPrintf(
      "When looking for directFromSellerSignalsHeaderAdSlot %s, failed to "
      "find a matching response.",
      ad_slot.c_str());
}

// All URLs received from worklets must be valid HTTPS URLs. It's up to callers
// to call ReportBadMessage() on invalid URLs.
bool IsUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

bool IsKAnon(const base::flat_set<std::string>& kanon_keys,
             const std::string& key) {
  return kanon_keys.contains(key);
}

// Verifies if the selectedBuyerAndSellerReportingId is present within the
// matching ad's selectableBuyerAndSellerReportingId array.
bool IsSelectedReportingIdValid(
    base::optional_ref<const std::vector<std::string>> selectable_ids,
    const std::string& selected_id) {
  if (!selectable_ids.has_value()) {
    return false;
  }
  return base::Contains(*selectable_ids, selected_id);
}

std::vector<auction_worklet::mojom::KAnonKeyPtr> KAnonKeysToMojom(
    const base::flat_set<std::string>& kanon_keys) {
  std::vector<auction_worklet::mojom::KAnonKeyPtr> result;
  for (const auto& key : kanon_keys) {
    result.emplace_back(auction_worklet::mojom::KAnonKey::New(key));
  }
  return result;
}

// Finds InterestGroup::Ad in `ads` that matches `ad_descriptor`, if any.
// Returns nullptr if `ad_descriptor` is invalid.
const blink::InterestGroup::Ad* FindMatchingAd(
    const std::vector<blink::InterestGroup::Ad>& ads,
    const base::flat_set<std::string>& kanon_keys,
    const blink::InterestGroup& interest_group,
    auction_worklet::mojom::BidRole bid_role,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id,
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

  if (bid_role != auction_worklet::mojom::BidRole::kUnenforcedKAnon) {
    const std::string kanon_key =
        is_component_ad
            ? blink::HashedKAnonKeyForAdComponentBid(ad_descriptor)
            : blink::HashedKAnonKeyForAdBid(interest_group, ad_descriptor);
    if (!IsKAnon(kanon_keys, kanon_key)) {
      return nullptr;
    }
  }

  const blink::InterestGroup::Ad* maybe_matching_ad = nullptr;
  for (const auto& ad : ads) {
    if (ad.render_url() != ad_descriptor.url) {
      continue;
    }
    if (!ad.size_group && !ad_descriptor.size) {
      // Neither `blink::InterestGroup::Ad` nor the ad from the bid have any
      // size specifications. They are considered as matching ad as long as
      // they have the same url.
      maybe_matching_ad = &ad;
      break;
    }
    if (!ad.size_group != !ad_descriptor.size) {
      // When only one of the two ads has a size specification, they are
      // considered matching. The caller is responsible for discarding the
      // extraneous size information
      maybe_matching_ad = &ad;
      break;
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
      maybe_matching_ad = &ad;
      break;
    }
  }

  if (maybe_matching_ad && !is_component_ad &&
      bid_role != auction_worklet::mojom::BidRole::kUnenforcedKAnon &&
      selected_buyer_and_seller_reporting_id.has_value()) {
    const std::string kanon_key = blink::HashedKAnonKeyForAdNameReporting(
        interest_group, *maybe_matching_ad,
        *selected_buyer_and_seller_reporting_id);
    if (!IsKAnon(kanon_keys, kanon_key)) {
      return nullptr;
    }
  }

  return maybe_matching_ad;
}

// Checks that `bid` is a valid bid value for an auction.
bool IsValidBid(double bid) {
  return !std::isnan(bid) && std::isfinite(bid) && bid > 0;
}

// Note that this only checks the structure, but does not verify k-anon of
// individual bids; TryToCreateBid does that.
bool IsValidMultiBid(
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    AuctionMetricsRecorder* auction_metrics_recorder,
    const std::vector<auction_worklet::mojom::BidderWorkletBidPtr>& mojo_bids,
    uint16_t multi_bid_limit) {
  unsigned kanon_bids = 0;
  unsigned non_kanon_bids = 0;

  for (const auto& bid : mojo_bids) {
    switch (bid->bid_role) {
      case auction_worklet::mojom::BidRole::kEnforcedKAnon:
        ++kanon_bids;
        break;

      case auction_worklet::mojom::BidRole::kUnenforcedKAnon:
        ++non_kanon_bids;
        break;

      case auction_worklet::mojom::BidRole::kBothKAnonModes:
        ++kanon_bids;
        ++non_kanon_bids;
        break;
    }
  }

  auction_metrics_recorder->RecordNumberOfBidsFromGenerateBid(kanon_bids,
                                                              mojo_bids.size());

  // There shouldn't be any k-anon bids if we're not dealing with it.
  if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kNone &&
      kanon_bids != 0) {
    return false;
  }

  // Shouldn't make more than `multi_bid_limit` bids in either auction.
  return (kanon_bids <= multi_bid_limit) && (non_kanon_bids <= multi_bid_limit);
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
    auction_worklet::mojom::BidRole bid_role) {
  if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kEnforce) {
    return bid_role != auction_worklet::mojom::BidRole::kUnenforcedKAnon;
  } else {
    return bid_role != auction_worklet::mojom::BidRole::kEnforcedKAnon;
  }
}

static const char* ScoreAdTraceEventName(const InterestGroupAuction::Bid& bid) {
  if (bid.bid_role == auction_worklet::mojom::BidRole::kEnforcedKAnon) {
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
std::optional<absl::uint128> BucketBaseForReportPaBuyers(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  if (!config.non_shared_params.auction_report_buyer_keys) {
    return std::nullopt;
  }
  // Find the index of the buyer in `buyers`. It should be present, since we
  // only load interest groups belonging to owners from `buyers`.
  CHECK(config.non_shared_params.interest_group_buyers);
  const std::vector<url::Origin>& buyers =
      *config.non_shared_params.interest_group_buyers;
  std::optional<size_t> index;
  for (size_t i = 0; i < buyers.size(); i++) {
    if (buyer == buyers.at(i)) {
      index = i;
      break;
    }
  }
  CHECK(index);
  // Use that index to get the associated bucket base, if present.
  if (*index >= config.non_shared_params.auction_report_buyer_keys->size()) {
    return std::nullopt;
  }
  return config.non_shared_params.auction_report_buyer_keys->at(*index);
}

// Helper for ReportPaBuyersValueIfAllowed() -- returns the
// AuctionReportBuyersConfig for `buyer_report_type`, if it exists in
// `auction_report_buyers` in `config`.
std::optional<blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>
ReportBuyersConfigForPaBuyers(
    blink::AuctionConfig::NonSharedParams::BuyerReportType buyer_report_type,
    const blink::AuctionConfig& config) {
  if (!config.non_shared_params.auction_report_buyers) {
    return std::nullopt;
  }
  const auto& report_buyers = *config.non_shared_params.auction_report_buyers;
  auto it = report_buyers.find(buyer_report_type);
  if (it == report_buyers.end()) {
    return std::nullopt;
  }
  return it->second;
}

// Helps compute various % utilization PA metrics. Caps at 110%, since not
// all caps are hard-enforced.
double PercentMetric(size_t numerator, size_t denominator) {
  if (denominator == 0) {
    return 0.0;
  }
  return std::min(100.0 * numerator / denominator, 110.0);
}

// Bounds to 1.1 of upper bound.
double SoftBound(size_t value, size_t cap) {
  return std::min(static_cast<double>(value), 1.1 * cap);
}

// This encodes which particular generateBid/scoreAd executions are to be used
// for "reserved.once" per given auction phase.
using PrivateAggregationReservedOnceReps =
    std::array<const InterestGroupAuction::BidState*,
               base::checked_cast<size_t>(PrivateAggregationPhase::kNumPhases)>;

// Takes private aggregation requests for `state`, if there are any, and moves
// them into `private_aggregation_requests_reserved` and
// `private_aggregation_requests_non_reserved`.
//
// Calculates bucket/value using `signals` and `top_level_signals` as needed.
//
// `winner` points to the BidState associated with the winning bid, if there
// is one.
//
// `signals` are the PostAuctionSignals from the auction `state` was a part of.
void TakePrivateAggregationRequestsForBidState(
    std::unique_ptr<InterestGroupAuction::BidState>& state,
    bool is_component_auction,
    const InterestGroupAuction::BidState* winner,
    const InterestGroupAuction::BidState* non_kanon_winner,
    const PrivateAggregationReservedOnceReps& reserved_once_reps,
    const InterestGroupAuction::PostAuctionSignals& signals,
    const std::optional<InterestGroupAuction::PostAuctionSignals>&
        top_level_signals,
    const InterestGroupAuction::PrivateAggregationAllParticipantsDataPtrs&
        all_participant_data,
    std::map<PrivateAggregationKey,
             InterestGroupAuctionReporter::PrivateAggregationRequests>&
        private_aggregation_requests_reserved,
    std::map<std::string,
             InterestGroupAuctionReporter::PrivateAggregationRequests>&
        private_aggregation_requests_non_reserved) {
  bool is_winner = state.get() == winner;
  for (auto& [key, requests] : state->private_aggregation_requests) {
    const url::Origin& origin = key.reporting_origin;
    PrivateAggregationPhase phase = key.phase;
    bool is_reserved_once_rep =
        state.get() == reserved_once_reps[static_cast<int>(phase)];
    const std::optional<url::Origin>& aggregation_coordinator_origin =
        key.aggregation_coordinator_origin;
    double winning_bid_to_use = signals.winning_bid;
    double highest_scoring_other_bid_to_use = signals.highest_scoring_other_bid;
    // When component auctions are in use, a BuyerHelper for a component
    // auction calls here for the scoreAd() aggregation calls from the
    // top-level; in that case the relevant signals are in
    // `top_level_signals` and not `signals`. `highest_scoring_other_bid`
    // is also not reported for top-levels.
    if (phase == PrivateAggregationPhase::kTopLevelSeller &&
        is_component_auction) {
      highest_scoring_other_bid_to_use = 0;
      winning_bid_to_use =
          top_level_signals.has_value() ? top_level_signals->winning_bid : 0.0;
    }

    const PrivateAggregationParticipantData* participant_data =
        all_participant_data[static_cast<size_t>(phase)];
    CHECK(participant_data);

    for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
         requests) {
      bool reserved_once = IsPrivateAggregationRequestReservedOnce(*request);
      if (reserved_once && !is_reserved_once_rep) {
        continue;
      }
      std::optional<PrivateAggregationRequestWithEventType> converted_request =
          FillInPrivateAggregationRequest(
              std::move(request), winning_bid_to_use,
              highest_scoring_other_bid_to_use, state->reject_reason,
              *participant_data, state->pa_timings(phase), is_winner);
      if (converted_request.has_value()) {
        PrivateAggregationRequestWithEventType converted_request_value =
            std::move(converted_request.value());
        const std::optional<std::string>& event_type =
            converted_request_value.event_type;
        if (event_type.has_value()) {
          // The request has a non-reserved event type.
          private_aggregation_requests_non_reserved[event_type.value()]
              .emplace_back(std::move(converted_request_value.request));
        } else {
          PrivateAggregationKey agg_key = {origin,
                                           aggregation_coordinator_origin};
          private_aggregation_requests_reserved[std::move(agg_key)]
              .emplace_back(std::move(converted_request_value.request));
        }
      }
    }
  }
  if (non_kanon_winner == state.get()) {
    bool is_reserved_once_rep =
        state.get() ==
        reserved_once_reps[static_cast<int>(PrivateAggregationPhase::kBidder)];

    const url::Origin& bidder = state->bidder->interest_group.owner;
    const std::optional<url::Origin>& aggregation_coordinator_origin =
        state->bidder->interest_group.aggregation_coordinator_origin;
    for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
         state->non_kanon_private_aggregation_requests) {
      bool reserved_once = IsPrivateAggregationRequestReservedOnce(*request);
      if (reserved_once && !is_reserved_once_rep) {
        continue;
      }

      const PrivateAggregationParticipantData* participant_data =
          all_participant_data[static_cast<size_t>(
              PrivateAggregationPhase::kBidder)];
      CHECK(participant_data);
      std::optional<PrivateAggregationRequestWithEventType> converted_request =
          FillInPrivateAggregationRequest(
              std::move(request), signals.winning_bid,
              signals.highest_scoring_other_bid,
              auction_worklet::mojom::RejectReason::kBelowKAnonThreshold,
              *participant_data,
              state->pa_timings(PrivateAggregationPhase::kBidder), false);
      if (converted_request.has_value()) {
        PrivateAggregationKey agg_key = {bidder,
                                         aggregation_coordinator_origin};
        PrivateAggregationRequestWithEventType converted_request_value =
            std::move(converted_request.value());
        // Only reserved types are supported for k-anon failures.
        // This *should* be guaranteed by `FillInPrivateAggregationRequest`
        // since we passed in `false` for `is_winner`.
        CHECK(!converted_request_value.event_type.has_value());
        private_aggregation_requests_reserved[std::move(agg_key)].emplace_back(
            std::move(converted_request_value.request));
      }
    }
  }

  // Also collect private aggregation requests from B&A server which no longer
  // needs to be filtered by Chrome, such as single seller auction, server
  // orchestrated auction, component losing buyer/seller requests.
  for (auto& [key, requests] : state->server_filtered_pagg_requests_reserved) {
    if (!requests.empty()) {
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_reserved[key];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
  for (auto& [event, requests] :
       state->server_filtered_pagg_requests_non_reserved) {
    if (!requests.empty()) {
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_non_reserved[event];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
}

void TakeRealTimeContributionsForBidState(
    InterestGroupAuction::BidState& state,
    std::map<url::Origin, InterestGroupAuction::RealTimeReportingContributions>&
        contributions) {
  for (auto& [origin, per_origin_contributions] :
       state.real_time_contributions) {
    InterestGroupAuction::RealTimeReportingContributions&
        contributions_for_origin = contributions[origin];
    contributions_for_origin.insert(
        contributions_for_origin.end(),
        std::move_iterator(per_origin_contributions.begin()),
        std::move_iterator(per_origin_contributions.end()));
  }
}

// Returns true if `origin` is in cooldown or lockout to send forDebuggingOnly
// reports. Ignore the cooldown or lockout if they started from a time which was
// earlier than kFledgeEnableFilteringDebugReportStartingFrom (i.e., before the
// time filtering debug report is enabled).
// TODO(crbug.com/362299758): For parameter `now`, use the same timestamp for
// all callers of the same auction.
bool IsOriginInDebugReportCooldownOrLockout(
    const url::Origin& origin,
    const std::optional<DebugReportLockoutAndCooldowns>&
        debug_report_lockout_and_cooldowns,
    const base::Time now) {
  if (!debug_report_lockout_and_cooldowns.has_value()) {
    return false;
  }

  if (IsInDebugReportLockout(
          debug_report_lockout_and_cooldowns->last_report_sent_time, now)) {
    return true;
  }

  const auto cooldown_it =
      debug_report_lockout_and_cooldowns->debug_report_cooldown_map.find(
          origin);
  if (cooldown_it !=
      debug_report_lockout_and_cooldowns->debug_report_cooldown_map.end()) {
    std::optional<base::TimeDelta> duration =
        ConvertDebugReportCooldownTypeToDuration(cooldown_it->second.type);
    if (duration.has_value()) {
      bool is_cooldown_before_filtering_starting =
          cooldown_it->second.starting_time <
          CeilToNearestNextHour(
              blink::features::kFledgeEnableFilteringDebugReportStartingFrom
                  .Get());
      bool is_in_cooldown =
          cooldown_it->second.starting_time + *duration >= now;
      if (!is_cooldown_before_filtering_starting && is_in_cooldown) {
        return true;
      }
    }
  }
  return false;
}

void UpdateDebugReportCooldown(
    const url::Origin& origin,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns,
    base::Time now_nearest_next_hour) {
  int restricted_cooldown_random_max =
      blink::features::kFledgeDebugReportSamplingRestrictedCooldownRandomMax
          .Get();
  CHECK_GE(restricted_cooldown_random_max, 0);
  // Give a restricted cooldown in 1/(restricted_cooldown_random_max+1)
  // chance. Treat INT_MAX `restricted_cooldown_random_max` as 0 chance.
  int cooldown_rand = base::RandInt(0, restricted_cooldown_random_max);
  DebugReportCooldownType cooldown_type =
      restricted_cooldown_random_max == INT_MAX || cooldown_rand != 0
          ? DebugReportCooldownType::kShortCooldown
          : DebugReportCooldownType::kRestrictedCooldown;
  base::UmaHistogramEnumeration(
      "Ads.InterestGroup.Auction.ForDebuggingOnlyCooldownType", cooldown_type);

  // Only set cooldown when `cooldown_type`'s cooldown length is not zero.
  if ((cooldown_type == DebugReportCooldownType::kShortCooldown &&
       blink::features::kFledgeDebugReportShortCooldown.Get() !=
           base::Milliseconds(0)) ||
      (cooldown_type == DebugReportCooldownType::kRestrictedCooldown &&
       blink::features::kFledgeDebugReportRestrictedCooldown.Get() !=
           base::Milliseconds(0))) {
    new_debug_report_lockout_and_cooldowns.debug_report_cooldown_map[origin] =
        DebugReportCooldown(now_nearest_next_hour, cooldown_type);
  }
}

// Samples forDebuggingOnly reports with a given sampling rate.
// `is_from_server_response` is true if it's a report from B&A response, which
// was already sampled by B&A server.
bool SampleDebugReport(
    const url::Origin& origin,
    bool is_from_server_response,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns) {
  bool can_send_debug_report = false;
  int sampling_random_max =
      blink::features::kFledgeDebugReportSamplingRandomMax.Get();
  CHECK_GE(sampling_random_max, 0);

  base::Time now_nearest_next_hour = base::Time::FromDeltaSinceWindowsEpoch(
      base::Time::Now().ToDeltaSinceWindowsEpoch().CeilToMultiple(
          base::Hours(1)));
  // Only allow sending debug reports 1/(sampling_max_rand+1) chance. Treat
  // INT_MAX `sampling_random_max` as 0 chance.
  int sampling_rand = base::RandInt(0, sampling_random_max);
  // Don't do sampling if the report is from B&A response, which has already
  // been sampled on server side.
  if (is_from_server_response ||
      (sampling_random_max != INT_MAX && sampling_rand == 0)) {
    can_send_debug_report = true;
    // Only set lockout when lockout length kFledgeDebugReportLockout is not
    // zero.
    if (blink::features::kFledgeDebugReportLockout.Get() !=
        base::Milliseconds(0)) {
      new_debug_report_lockout_and_cooldowns.last_report_sent_time =
          now_nearest_next_hour;
    }
  }
  base::UmaHistogramBoolean(
      "Ads.InterestGroup.Auction.ForDebuggingOnlyReportAllowedAfterSampling",
      can_send_debug_report);

  UpdateDebugReportCooldown(origin, new_debug_report_lockout_and_cooldowns,
                            now_nearest_next_hour);

  return can_send_debug_report;
}

// Returns whether to keep the debug report or not. Returns true if flag
// kFledgeSampleDebugReports is disabled, or sampling allows sending the report,
// or kFledgeEnableFilteringDebugReportStartingFrom is zero.
// `is_from_server_response` is true if it's a report from B&A response, which
// was already sampled by B&A server.
bool KeepDebugReport(
    const url::Origin& origin,
    bool is_from_server_response,
    std::optional<DebugReportLockoutAndCooldowns>&
        debug_report_lockout_and_cooldowns,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeSampleDebugReports)) {
    return true;
  }

  bool can_send_debug_report = false;
  base::Time now = base::Time::Now();
  if (!IsOriginInDebugReportCooldownOrLockout(
          origin, debug_report_lockout_and_cooldowns, now) &&
      !IsOriginInDebugReportCooldownOrLockout(
          origin, new_debug_report_lockout_and_cooldowns, now)) {
    // `SampleDebugReport()` may modify the lockout and cooldown state.
    can_send_debug_report =
        SampleDebugReport(origin, is_from_server_response,
                          new_debug_report_lockout_and_cooldowns);
  }
  bool filter_enabled =
      blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get() !=
      base::Milliseconds(0);
  return !filter_enabled || can_send_debug_report;
}

// Helper function of TakeDebugReportUrlsForBidState(). Adds debug reporting
// URLs for `winner` to `debug_win_report_urls`, if there are any.
void TakeDebugReportUrlsForWinner(
    const InterestGroupAuction::BidState* winner,
    const InterestGroupAuction::PostAuctionSignals& signals,
    const std::optional<InterestGroupAuction::PostAuctionSignals>&
        top_level_signals,
    const url::Origin& bidder,
    const url::Origin& seller,
    const std::optional<url::Origin>& top_level_seller,
    std::optional<DebugReportLockoutAndCooldowns>&
        debug_report_lockout_and_cooldowns,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns,
    std::vector<GURL>& debug_win_report_urls) {
  if (winner->bidder_debug_win_report_url.has_value() &&
      KeepDebugReport(bidder, winner->is_from_server_response,
                      debug_report_lockout_and_cooldowns,
                      new_debug_report_lockout_and_cooldowns)) {
    debug_win_report_urls.emplace_back(
        InterestGroupAuction::FillPostAuctionSignals(
            std::move(winner->bidder_debug_win_report_url).value(), signals));
  }
  if (winner->seller_debug_win_report_url.has_value() &&
      KeepDebugReport(seller, winner->is_from_server_response,
                      debug_report_lockout_and_cooldowns,
                      new_debug_report_lockout_and_cooldowns)) {
    debug_win_report_urls.emplace_back(
        InterestGroupAuction::FillPostAuctionSignals(
            std::move(winner->seller_debug_win_report_url).value(), signals,
            top_level_signals));
  }
  if (winner->top_level_seller_debug_win_report_url.has_value()) {
    CHECK(top_level_seller.has_value());
    if (KeepDebugReport(*top_level_seller, winner->is_from_server_response,
                        debug_report_lockout_and_cooldowns,
                        new_debug_report_lockout_and_cooldowns)) {
      // `top_level_signals` is passed as parameter `signals` for top-level
      // seller.
      debug_win_report_urls.emplace_back(
          InterestGroupAuction::FillPostAuctionSignals(
              std::move(winner->top_level_seller_debug_win_report_url).value(),
              top_level_signals.value()));
    }
  }
}

// Helper function of TakeDebugReportUrlsForBidState(). Adds debug reporting
// URLs for losing `bid_state` to `debug_loss_report_urls`, if there are any.
void TakeDebugReportUrlsForLosingBidState(
    std::unique_ptr<InterestGroupAuction::BidState>& bid_state,
    const InterestGroupAuction::PostAuctionSignals& signals,
    const std::optional<InterestGroupAuction::PostAuctionSignals>&
        top_level_signals,
    const url::Origin& bidder,
    const url::Origin& seller,
    const std::optional<url::Origin>& top_level_seller,
    std::optional<DebugReportLockoutAndCooldowns>&
        debug_report_lockout_and_cooldowns,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns,
    std::vector<GURL>& debug_loss_report_urls) {
  if (bid_state->bidder_debug_loss_report_url.has_value() &&
      KeepDebugReport(bidder, bid_state->is_from_server_response,
                      debug_report_lockout_and_cooldowns,
                      new_debug_report_lockout_and_cooldowns)) {
    // Losing and rejected bidders should not get highest_scoring_other_bid
    // and made_highest_scoring_other_bid signals. (And also the currency bit
    // for those).
    debug_loss_report_urls.emplace_back(
        InterestGroupAuction::FillPostAuctionSignals(
            std::move(bid_state->bidder_debug_loss_report_url).value(),
            InterestGroupAuction::PostAuctionSignals(
                signals.winning_bid, signals.winning_bid_currency,
                signals.made_winning_bid, /*highest_scoring_other_bid=*/0.0,
                /*highest_scoring_other_bid_currency=*/std::nullopt,
                /*made_highest_scoring_other_bid=*/false),
            /*top_level_signals=*/std::nullopt, bid_state->reject_reason));
  }
  if (bid_state->seller_debug_loss_report_url.has_value() &&
      KeepDebugReport(seller, bid_state->is_from_server_response,
                      debug_report_lockout_and_cooldowns,
                      new_debug_report_lockout_and_cooldowns)) {
    // TODO(qingxinwu): Add reject reason to seller debug loss report as well.
    debug_loss_report_urls.emplace_back(
        InterestGroupAuction::FillPostAuctionSignals(
            std::move(bid_state->seller_debug_loss_report_url).value(), signals,
            top_level_signals));
  }

  if (bid_state->top_level_seller_debug_loss_report_url.has_value()) {
    CHECK(top_level_seller.has_value());
    if (KeepDebugReport(*top_level_seller, bid_state->is_from_server_response,
                        debug_report_lockout_and_cooldowns,
                        new_debug_report_lockout_and_cooldowns)) {
      // `top_level_signals` is passed as parameter `signals` for top-level
      // seller.
      debug_loss_report_urls.emplace_back(
          InterestGroupAuction::FillPostAuctionSignals(
              std::move(bid_state->top_level_seller_debug_loss_report_url)
                  .value(),
              top_level_signals.value()));
    }
  }
}

// Adds debug reporting URLs for `bid_state` to `debug_win_report_urls` and
// `debug_loss_report_urls`, if there are any, filling in report URL template
// parameters as needed. The URLs are moved away from `bid_state`.
//
// `winner` points to the BidState associated with the winning bid, if there
// is one.
//
// `signals` are the PostAuctionSignals from the auction `bid_state` was a part
// of.
//
// `top_level_signals` are the PostAuctionSignals of the top-level auction, if
// the computation is for a component auction, and nullopt otherwise.
//
// `bidder` is the buyer who owns `bid_state`.
//
// `seller` is the seller of the auction `bid_state` was a part of.
//
// `top_level_seller` should be set for component auctions only.
//
// `debug_report_lockout_and_cooldowns` is lockout and cooldowns read from DB
// when the auction started.
//
// `new_debug_report_lockout_and_cooldowns` is lockout and cooldowns generated
// from this auction.
void TakeDebugReportUrlsForBidState(
    std::unique_ptr<InterestGroupAuction::BidState>& bid_state,
    const InterestGroupAuction::BidState* winner,
    const InterestGroupAuction::PostAuctionSignals& signals,
    const std::optional<InterestGroupAuction::PostAuctionSignals>&
        top_level_signals,
    const url::Origin& bidder,
    const url::Origin& seller,
    const std::optional<url::Origin>& top_level_seller,
    std::optional<DebugReportLockoutAndCooldowns>&
        debug_report_lockout_and_cooldowns,
    DebugReportLockoutAndCooldowns& new_debug_report_lockout_and_cooldowns,
    std::vector<GURL>& debug_win_report_urls,
    std::vector<GURL>& debug_loss_report_urls) {
  base::Time now = base::Time::Now();
  base::Time now_nearest_next_hour = base::Time::FromDeltaSinceWindowsEpoch(
      now.ToDeltaSinceWindowsEpoch().CeilToMultiple(base::Hours(1)));
  // TODO(qingxinwu): Give bidder's and seller's debug report the same chance to
  // be kept after sampling. Bidder's debug report is sampled before seller's,
  // giving bidder's report a higher chance to be kept (especially when the
  // sample rate is high), which seems unfair to the seller.
  if (bid_state.get() == winner) {
    TakeDebugReportUrlsForWinner(
        winner, signals, top_level_signals, bidder, seller, top_level_seller,
        debug_report_lockout_and_cooldowns,
        new_debug_report_lockout_and_cooldowns, debug_win_report_urls);
  } else {
    TakeDebugReportUrlsForLosingBidState(
        bid_state, signals, top_level_signals, bidder, seller, top_level_seller,
        debug_report_lockout_and_cooldowns,
        new_debug_report_lockout_and_cooldowns, debug_loss_report_urls);
  }

  // For server filtered fDO reports from a B&A auction.
  for (const auto& [origin, reportUrls] :
       bid_state->server_filtered_debugging_only_reports) {
    if (reportUrls.empty()) {
      if (!IsOriginInDebugReportCooldownOrLockout(
              origin, debug_report_lockout_and_cooldowns, now) &&
          !IsOriginInDebugReportCooldownOrLockout(
              origin, new_debug_report_lockout_and_cooldowns, now)) {
        UpdateDebugReportCooldown(origin,
                                  new_debug_report_lockout_and_cooldowns,
                                  now_nearest_next_hour);
      }
      continue;
    }
    for (const auto& report : reportUrls) {
      // Server filtered debug reports have been sampled on B&A servers already,
      // so do not run sampling on client again for these reports.
      if (KeepDebugReport(origin, /*is_from_server_response=*/true,
                          debug_report_lockout_and_cooldowns,
                          new_debug_report_lockout_and_cooldowns)) {
        // For server filtered ones, post auction signals should have been
        // filled on the server side. And as a result, for server filtered ones,
        // there's no difference if they go to loss or win lists, since the
        // difference was post auction signals.
        debug_loss_report_urls.emplace_back(report);
      }
    }
  }
}

// Retrieves the timeout from `buyer_timeouts` associated with `buyer`, if any.
// Used for both `buyer_timeouts` and `buyer_cumulative_timeouts`, stored in
// AuctionConfigs. Callers should use PerBuyerTimeout() and
// PerBuyerCumulativeTimeout() instead, since those apply the timeout limit,
// when applicable.
std::optional<base::TimeDelta> PerBuyerTimeoutHelper(
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
  return std::nullopt;
}

std::optional<base::TimeDelta> PerBuyerTimeout(
    const url::Origin& buyer,
    const blink::AuctionConfig& auction_config) {
  std::optional<base::TimeDelta> out = PerBuyerTimeoutHelper(
      buyer, auction_config.non_shared_params.buyer_timeouts);
  if (!out) {
    return out;
  }
  return std::min(*out, kMaxPerBuyerTimeout);
}

std::optional<base::TimeDelta> PerBuyerCumulativeTimeout(
    const url::Origin& buyer,
    const blink::AuctionConfig& auction_config) {
  return PerBuyerTimeoutHelper(
      buyer, auction_config.non_shared_params.buyer_cumulative_timeouts);
}

std::optional<blink::AdCurrency> PerBuyerCurrency(
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

// PA Mojo validation common to generateBid and scoreAd result.
// If they're wrong, calls ReportBadMessage and returns false.
template <typename MojoReceiver>
bool ValidatePrivateAggregationRequests(
    MojoReceiver& receiver,
    const PrivateAggregationRequests& pa_requests) {
  std::optional<std::string> error =
      content::ValidatePrivateAggregationRequests(pa_requests);
  if (error.has_value()) {
    receiver.ReportBadMessage(*error);
    return false;
  }
  return true;
}

// PA Mojo validation for generateBid, adding some additional checks for
// `non_kanon_pa_requests`.
//
// If they're wrong, calls ReportBadMessage and returns false.
bool ValidateBidderPrivateAggregationRequests(
    mojo::AssociatedReceiverSet<auction_worklet::mojom::GenerateBidClient,
                                InterestGroupAuction::BidState*>&
        generate_bid_client_receiver_set,
    const PrivateAggregationRequests& pa_requests,
    const PrivateAggregationRequests& non_kanon_pa_requests) {
  if (!ValidatePrivateAggregationRequests(generate_bid_client_receiver_set,
                                          pa_requests)) {
    return false;
  }

  if (!ValidatePrivateAggregationRequests(generate_bid_client_receiver_set,
                                          non_kanon_pa_requests)) {
    return false;
  }

  for (const auto& non_kanon_request : non_kanon_pa_requests) {
    if (!auction_worklet::HasKAnonFailureComponent(*non_kanon_request)) {
      generate_bid_client_receiver_set.ReportBadMessage(
          "Incorrect non-kanon Private Aggregation request");
      return false;
    }
  }

  return true;
}

}  // namespace

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals() = default;

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals(
    double winning_bid,
    std::optional<blink::AdCurrency> winning_bid_currency,
    bool made_winning_bid)
    : winning_bid(winning_bid),
      winning_bid_currency(std::move(winning_bid_currency)),
      made_winning_bid(made_winning_bid) {}

InterestGroupAuction::PostAuctionSignals::PostAuctionSignals(
    double winning_bid,
    std::optional<blink::AdCurrency> winning_bid_currency,
    bool made_winning_bid,
    double highest_scoring_other_bid,
    std::optional<blink::AdCurrency> highest_scoring_other_bid_currency,
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
    std::optional<url::Origin> winner_owner,
    double winning_bid,
    std::optional<double> winning_bid_in_seller_currency,
    const std::optional<blink::AdCurrency>& seller_currency,
    bool& out_made_winning_bid,
    double& out_winning_bid,
    std::optional<blink::AdCurrency>& out_winning_bid_currency) {
  out_made_winning_bid = false;
  if (winner_owner.has_value()) {
    out_made_winning_bid = owner == *winner_owner;
  }

  if (seller_currency.has_value()) {
    out_winning_bid = winning_bid_in_seller_currency.value_or(0.0);
    out_winning_bid_currency = *seller_currency;
  } else {
    out_winning_bid = winning_bid;
    out_winning_bid_currency = std::nullopt;
  }
}

// static
void InterestGroupAuction::PostAuctionSignals::
    FillRelevantHighestScoringOtherBidInfo(
        const url::Origin& owner,
        std::optional<url::Origin> highest_scoring_other_bid_owner,
        double highest_scoring_other_bid,
        std::optional<double> highest_scoring_other_bid_in_seller_currency,
        const std::optional<blink::AdCurrency>& seller_currency,
        bool& out_made_highest_scoring_other_bid,
        double& out_highest_scoring_other_bid,
        std::optional<blink::AdCurrency>&
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
    out_highest_scoring_other_bid_currency = std::nullopt;
  }
}

InterestGroupAuction::BidState::~BidState() {
  if (trace_id.has_value()) {
    EndTracing();
  }
}

InterestGroupAuction::BidState::BidState(BidState&&) = default;

InterestGroupAuction::BidState::BidState(
    const SingleStorageInterestGroup&& bidder)
    : bidder(std::move(bidder)) {}

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
  trace_id = std::nullopt;
}

InterestGroupAuction::Bid::Bid(
    auction_worklet::mojom::BidRole bid_role,
    std::string ad_metadata,
    double bid,
    std::optional<blink::AdCurrency> bid_currency,
    std::optional<double> ad_cost,
    blink::AdDescriptor ad_descriptor,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    std::optional<uint16_t> modeling_signals,
    base::TimeDelta bid_duration,
    std::optional<uint32_t> bidding_signals_data_version,
    const blink::InterestGroup::Ad* bid_ad,
    std::optional<std::string> selected_buyer_and_seller_reporting_id,
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
      selected_buyer_and_seller_reporting_id(
          std::move(selected_buyer_and_seller_reporting_id)),
      interest_group(&bid_state->bidder->interest_group),
      bid_ad(bid_ad),
      bid_state(bid_state),
      auction(auction) {
  DCHECK(IsValidBid(bid));
}

InterestGroupAuction::Bid::Bid(Bid&) = default;

InterestGroupAuction::Bid::~Bid() {
  if (trace_id.has_value()) {
    EndTracingForScoring();
  }
}

void InterestGroupAuction::Bid::BeginTracingForScoring() {
  DCHECK(!trace_id.has_value());

  trace_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "fledge", "score", *trace_id, "bidding_url", interest_group->bidding_url,
      "interest_group_name", interest_group->name);
}

void InterestGroupAuction::Bid::EndTracingForScoring() {
  DCHECK(trace_id.has_value());

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "score", *trace_id);
  trace_id = std::nullopt;
}

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

// If the auction config specified 'deprecatedRenderURLReplacements', this will
// return the ad descriptor with the proper replacements.
blink::AdDescriptor
InterestGroupAuction::Bid::GetAdDescriptorWithReplacements() {
  std::vector<std::pair<std::string, std::string>> local_replacements;
  // Convert `replacements` into a vector of pairs to use within
  // `SubstituteMappedStrings`.
  for (const auto& replacement :
       auction->GetDeprecatedRenderURLReplacements()) {
    local_replacements.emplace_back(replacement.match, replacement.replacement);
  }
  GURL url_with_replacements = GURL(
      SubstituteMappedStrings(ad_descriptor.url.spec(), local_replacements));

  if (url_with_replacements.is_valid()) {
    return blink::AdDescriptor(GURL(std::move(url_with_replacements)),
                               ad_descriptor.size);
  }
  return ad_descriptor;
}

// If the auction config specified 'deprecatedRenderURLReplacements', this will
// return the ad descriptors with the proper replacements.
std::vector<blink::AdDescriptor>
InterestGroupAuction::Bid::GetComponentAdDescriptorsWithReplacements() {
  std::vector<blink::AdDescriptor> local_component_ad_descriptors;
  std::vector<std::pair<std::string, std::string>> local_replacements;
  // Convert `replacements` into a vector of pairs to use within
  // `SubstituteMappedStrings`.
  for (const auto& replacement :
       auction->GetDeprecatedRenderURLReplacements()) {
    local_replacements.emplace_back(replacement.match, replacement.replacement);
  }

  for (auto& ad_component_descriptor : ad_component_descriptors) {
    GURL url_with_replacements = GURL(SubstituteMappedStrings(
        ad_component_descriptor.url.spec(), local_replacements));

    if (url_with_replacements.is_valid()) {
      local_component_ad_descriptors.emplace_back(
          GURL(std::move(url_with_replacements)), ad_component_descriptor.size);
    } else {
      local_component_ad_descriptors.emplace_back(ad_component_descriptor);
    }
  }
  return local_component_ad_descriptors;
}

InterestGroupAuction::ScoredBid::ScoredBid(
    double score,
    std::optional<uint32_t> scoring_signals_data_version,
    std::unique_ptr<Bid> bid,
    std::optional<double> bid_in_seller_currency,
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
              std::vector<SingleStorageInterestGroup>&& interest_groups)
      : auction_(auction),
        owner_(interest_groups[0]->interest_group.owner),
        multi_bid_limit_(auction_->GetBuyerMultiBidLimit(owner_)) {
    DCHECK(!interest_groups.empty());

    // Move interest groups to `bid_states_` and update priorities using
    // `priority_vector`, if present. Delete groups where the calculation
    // results in a priority < 0.
    for (SingleStorageInterestGroup& bidder : interest_groups) {
      double priority = bidder->interest_group.priority;

      if (bidder->interest_group.priority_vector &&
          !bidder->interest_group.priority_vector->empty()) {
        priority = CalculateInterestGroupPriority(
            *auction_->config_, *bidder, auction_->auction_start_time_,
            *bidder->interest_group.priority_vector);
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

      if (bidder->interest_group.enable_bidding_signals_prioritization) {
        enable_bidding_signals_prioritization_ = true;
      }
      auto state = std::make_unique<BidState>(std::move(bidder));
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

    // Determine the number of threads to allocate to the bidders. Generally,
    // use more threads for larger `bid_states_`. The size of `bid_states_`
    // correlates with the expected number of GenerateBid() calls, and thus, it
    // guides the multi-threading decision.
    //
    // Note: `number_of_bidder_threads` only applies when `RequestWorkletByKey`
    // ends up creating a new worklet; existing worklets retain their own
    // threading configuration.
    size_t number_of_bidder_threads = 1;
    if (num_outstanding_bids_ > 0) {
      number_of_bidder_threads += static_cast<int>(
          features::kFledgeBidderWorkletThreadPoolSizeLogarithmicScalingFactor
              .Get() *
          std::log10(num_outstanding_bids_));
    }

    // Limit thread count to 10. With log10, it's unlikely to get ridiculously
    // large, but enforce a maximum for safety.
    number_of_bidder_threads = std::min<size_t>(number_of_bidder_threads, 10);

    // Request processes for all bidder worklets.
    for (auto& bid_state : bid_states_) {
      auto worklet_key = auction_->BidderWorkletKey(*bid_state);
      auction_->auction_metrics_recorder_->ReportBidderWorkletKey(worklet_key);
      auction_->auction_worklet_manager_->RequestWorkletByKey(
          worklet_key, auction_->devtools_auction_id_,
          base::BindOnce(&BuyerHelper::OnBidderWorkletReceived,
                         base::Unretained(this), bid_state.get()),
          base::BindOnce(&BuyerHelper::OnBidderWorkletGenerateBidFatalError,
                         base::Unretained(this), bid_state.get()),
          bid_state->worklet_handle, number_of_bidder_threads,
          auction_->auction_metrics_recorder_);
    }
  }

  // auction_worklet::mojom::GenerateBidClient implementation:

  void OnBiddingSignalsReceived(
      const base::flat_map<std::string, double>& priority_vector,
      base::TimeDelta trusted_signals_fetch_latency,
      std::optional<base::TimeDelta> update_if_older_than,
      base::OnceClosure resume_generate_bid_callback) override {
    BidState* state = generate_bid_client_receiver_set_.current_context();
    const blink::InterestGroup& interest_group = state->bidder->interest_group;
    state->pa_timings(PrivateAggregationPhase::kBidder).signals_fetch_time =
        trusted_signals_fetch_latency;
    auction_->ReportTrustedSignalsFetchLatency(interest_group,
                                               trusted_signals_fetch_latency);
    auction_->HandleUpdateIfOlderThan(interest_group.owner, interest_group.name,
                                      update_if_older_than);
    std::optional<double> new_priority;
    if (!priority_vector.empty()) {
      new_priority = CalculateInterestGroupPriority(
          *auction_->config_, *(state->bidder), auction_->auction_start_time_,
          priority_vector,
          (interest_group.priority_vector &&
           !interest_group.priority_vector->empty())
              ? state->calculated_priority
              : std::optional<double>());
      if (*new_priority < 0) {
        auction_->auction_metrics_recorder_
            ->RecordBidFilteredDuringReprioritization();
      }
    }
    OnBiddingSignalsReceivedInternal(state, new_priority,
                                     std::move(resume_generate_bid_callback));
  }

  void OnGenerateBidComplete(
      std::vector<auction_worklet::mojom::BidderWorkletBidPtr> mojo_bids,
      std::optional<uint32_t> bidding_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      std::optional<double> set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      PrivateAggregationRequests non_kanon_pa_requests,
      RealTimeReportingContributions real_time_contributions,
      auction_worklet::mojom::BidderTimingMetricsPtr generate_bid_metrics,
      auction_worklet::mojom::GenerateBidDependencyLatenciesPtr
          generate_bid_dependency_latencies,
      auction_worklet::mojom::RejectReason reject_reason,
      const std::vector<std::string>& errors) override {
    BidState* state = generate_bid_client_receiver_set_.current_context();
    const blink::InterestGroup& interest_group = state->bidder->interest_group;
    state->pa_timings(PrivateAggregationPhase::kBidder).script_run_time =
        generate_bid_metrics->script_latency;
    auction_->ReportBiddingLatency(interest_group,
                                   generate_bid_metrics->script_latency);
    if (generate_bid_metrics->js_fetch_latency.has_value()) {
      code_fetch_time_.RecordLatency(*generate_bid_metrics->js_fetch_latency);
    }
    if (generate_bid_metrics->wasm_fetch_latency.has_value()) {
      code_fetch_time_.RecordLatency(*generate_bid_metrics->wasm_fetch_latency);
    }
    if (generate_bid_metrics->script_timed_out) {
      ++bidder_scripts_timed_out_;
    }

    // This is intentionally recorded here as opposed to in
    // OnGenerateBidCompleteInternal in order to exclude bids that were
    // filtered during reprioritization. It also excludes those bids that
    // encountered a fatal error, except for timeouts; those we record to this
    // metric separately and explicitly in OnTimeout.
    auction_->auction_metrics_recorder_->RecordBidForOneInterestGroupLatency(
        base::TimeTicks::Now() - start_generating_bids_time_);
    auction_->auction_metrics_recorder_->RecordGenerateBidDependencyLatencies(
        *generate_bid_dependency_latencies);
    OnGenerateBidCompleteInternal(
        state, std::move(mojo_bids), bidding_signals_data_version,
        debug_loss_report_url, debug_win_report_url, set_priority,
        std::move(update_priority_signals_overrides), std::move(pa_requests),
        std::move(non_kanon_pa_requests), std::move(real_time_contributions),
        reject_reason, errors);
  }

  void SetForDebuggingOnlyInCooldownOrLockout(
      bool for_debugging_only_in_cooldown_or_lockout) {
    for (auto& bid_state : bid_states_) {
      bid_state->bidder->bidding_browser_signals
          ->for_debugging_only_in_cooldown_or_lockout =
          for_debugging_only_in_cooldown_or_lockout;
    }
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
    stop_measuring_cumulative_time_ = base::TimeTicks::Now();
  }

  // Returns true if this buyer has any interest groups that will potentially
  // bid in an auction -- that is, not all interest groups have been filtered
  // out.
  bool has_potential_bidder() const { return !bid_states_.empty(); }

  size_t num_potential_bidders() const { return bid_states_.size(); }

  const url::Origin& owner() const { return owner_; }

  const PrivateAggregationParticipantData& buyer_metrics() const {
    return buyer_metrics_;
  }

  void FillInBidderParticipantDataMetrics() {
    if (code_fetch_time_.GetNumRecords() != 0) {
      buyer_metrics_.average_code_fetch_time =
          code_fetch_time_.GetMeanLatency();
    }
    buyer_metrics_.participating_interest_group_count = bid_states_.size();
    buyer_metrics_.percent_scripts_timeout =
        PercentMetric(bidder_scripts_timed_out_,
                      buyer_metrics_.participating_interest_group_count);
    buyer_metrics_.percent_igs_cumulative_timeout =
        PercentMetric(num_bids_affected_by_cumulative_timeout_,
                      buyer_metrics_.participating_interest_group_count);

    // We compute `cumulative-buyer-time` based on when we start the timer;
    // `cumulative_buyer_timeout_` and `start_measuring_cumulative_time_` will
    // be set then.
    if (cumulative_buyer_timeout_triggered_) {
      // If the timeout has triggered, its timer must have been started, so
      // `cumulative_buyer_timeout_` has been set. We report the timeout
      // value + 1000ms to make it clear in the metrics that a timeout happened.
      buyer_metrics_.cumulative_buyer_time =
          *cumulative_buyer_timeout_ + base::Milliseconds(1000);
    } else if (cumulative_buyer_timeout_.has_value() &&
               stop_measuring_cumulative_time_ >=
                   start_measuring_cumulative_time_) {
      // Here, the timer been started and stopped w/o it firing. We report the
      // elapsed time capped to the timeout. The cap makes this case numerically
      // distinct from the timeout case even with somewhat coarse buckets.
      buyer_metrics_.cumulative_buyer_time = std::min(
          stop_measuring_cumulative_time_ - start_measuring_cumulative_time_,
          *cumulative_buyer_timeout_);
    }
  }

  void SetStorageMetrics(int regular_igs,
                         int negative_igs,
                         size_t igs_storage_used) {
    buyer_metrics_.regular_igs = SoftBound(
        regular_igs, InterestGroupStorage::MaxOwnerRegularInterestGroups());
    buyer_metrics_.percent_regular_igs_quota_used = PercentMetric(
        regular_igs, InterestGroupStorage::MaxOwnerRegularInterestGroups());
    buyer_metrics_.negative_igs = SoftBound(
        negative_igs, InterestGroupStorage::MaxOwnerNegativeInterestGroups());
    buyer_metrics_.percent_negative_igs_quota_used = PercentMetric(
        negative_igs, InterestGroupStorage::MaxOwnerNegativeInterestGroups());
    buyer_metrics_.igs_storage_used = SoftBound(
        igs_storage_used, InterestGroupStorage::MaxOwnerStorageSize());
    buyer_metrics_.percent_igs_storage_quota_used = PercentMetric(
        igs_storage_used, InterestGroupStorage::MaxOwnerStorageSize());
  }

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
  //
  // `seller` is the seller of the auction `winner` was a part of.
  //
  // `top_level_seller` should be set for component auctions only.
  void TakeDebugReportUrls(
      const BidState* winner,
      const PostAuctionSignals& signals,
      const std::optional<PostAuctionSignals>& top_level_signals,
      const url::Origin& seller,
      const std::optional<url::Origin>& top_level_seller,
      std::vector<GURL>& debug_win_report_urls,
      std::vector<GURL>& debug_loss_report_urls) {
    for (std::unique_ptr<BidState>& bid_state : bid_states_) {
      TakeDebugReportUrlsForBidState(
          bid_state, winner, signals, top_level_signals, owner_, seller,
          top_level_seller, auction_->debug_report_lockout_and_cooldowns_,
          auction_->new_debug_report_lockout_and_cooldowns_,
          debug_win_report_urls, debug_loss_report_urls);
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
      const std::optional<PostAuctionSignals>& top_level_signals,
      const BidState* non_top_level_seller_once_rep,
      const BidState* top_level_seller_once_rep,
      const PrivateAggregationParticipantData* non_top_level_seller_data,
      const PrivateAggregationParticipantData* top_level_seller_data,
      std::map<PrivateAggregationKey, PrivateAggregationRequests>&
          private_aggregation_requests_reserved,
      std::map<std::string, PrivateAggregationRequests>&
          private_aggregation_requests_non_reserved) {
    if (bid_states_.empty()) {
      return;
    }

    FillInBidderParticipantDataMetrics();

    PrivateAggregationReservedOnceReps reps;
    PrivateAggregationAllParticipantsDataPtrs all_participant_data;
    reps[static_cast<size_t>(PrivateAggregationPhase::kTopLevelSeller)] =
        top_level_seller_once_rep;
    all_participant_data[static_cast<size_t>(
        PrivateAggregationPhase::kTopLevelSeller)] = top_level_seller_data;
    reps[static_cast<size_t>(PrivateAggregationPhase::kNonTopLevelSeller)] =
        non_top_level_seller_once_rep;
    all_participant_data[static_cast<size_t>(
        PrivateAggregationPhase::kNonTopLevelSeller)] =
        non_top_level_seller_data;
    all_participant_data[static_cast<size_t>(
        PrivateAggregationPhase::kBidder)] = &buyer_metrics_;

    // Figure out which bidder rep to use, out of those that didn't get blocked
    // by cumulative timeout.
    if (bid_states_.size() != num_bids_affected_by_cumulative_timeout_) {
      CHECK_LT(num_bids_affected_by_cumulative_timeout_, bid_states_.size());
      uint64_t skip = base::RandGenerator(
          bid_states_.size() - num_bids_affected_by_cumulative_timeout_);
      uint64_t pos = 0;

      while (true) {
        while (bid_states_[pos]->affected_by_cumulative_timeout) {
          ++pos;
        }
        if (skip == 0) {
          break;
        }
        --skip;
        ++pos;
      }

      reps[static_cast<size_t>(PrivateAggregationPhase::kBidder)] =
          bid_states_[pos].get();
    } else {
      reps[static_cast<size_t>(PrivateAggregationPhase::kBidder)] = nullptr;
    }

    for (std::unique_ptr<BidState>& state : bid_states_) {
      TakePrivateAggregationRequestsForBidState(
          state, /*is_component_auction=*/auction_->parent_, winner,
          non_kanon_winner, reps, signals, top_level_signals,
          all_participant_data, private_aggregation_requests_reserved,
          private_aggregation_requests_non_reserved);
    }
  }

  // Takes real time contributions and saves to `contributions`, if there are
  // any.
  void TakeRealTimeContributions(
      std::map<url::Origin,
               InterestGroupAuction::RealTimeReportingContributions>&
          contributions) {
    for (std::unique_ptr<BidState>& state : bid_states_) {
      TakeRealTimeContributionsForBidState(*state, contributions);
    }
  }

  void NotifyConfigPromisesResolved() {
    DCHECK(auction_->config_promises_resolved_);

    NotifyConfigDependencyResolved();
  }

  // Called when promises resolve, or directFromSellerSignalsHeaderAdSlot
  // finishes parsing and finding a matching ad slot response.
  void NotifyConfigDependencyResolved() {
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

  std::unique_ptr<Bid> TryToCreateBidFromServerResponse(
      auction_worklet::mojom::BidRole bid_role,
      double bid,
      const std::optional<blink::AdCurrency>& bid_currency,
      const std::optional<std::string>& ad_metadata,
      const std::optional<std::string>& buyer_reporting_id,
      const std::optional<std::string>& buyer_and_seller_reporting_id,
      blink::AdDescriptor ad_descriptor,
      std::vector<blink::AdDescriptor> ad_component_descriptors,
      std::map<PrivateAggregationPhaseKey, PrivateAggregationRequests>&
          component_win_pagg_requests,
      std::map<PrivateAggregationKey, PrivateAggregationRequests>&
          server_filtered_pagg_requests_reserved,
      std::map<std::string, PrivateAggregationRequests>&
          server_filtered_pagg_requests_non_reserved,
      std::map<BiddingAndAuctionResponse::DebugReportKey, std::optional<GURL>>&
          component_win_debugging_only_reports,
      std::map<url::Origin, std::vector<GURL>>&
          server_filtered_debugging_only_reports) {
    CHECK_EQ(1u, bid_states_.size());
    BidState* bid_state = bid_states_[0].get();
    bid_state->made_bid = true;
    bid_state->is_from_server_response = true;

    auction_worklet::mojom::KAnonymityBidMode kanon_mode =
        auction_->kanon_mode();
    bid_state->kanon_keys = ComputeKAnon(bid_state->bidder, kanon_mode);

    // Check bid validity.
    const blink::InterestGroup& interest_group =
        bid_state->bidder->interest_group;

    // 1. Ads must be in the interest group (at specified k-anon level)
    const blink::InterestGroup::Ad* matching_ad = FindMatchingAd(
        *interest_group.ads, bid_state->kanon_keys, interest_group, bid_role,
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
        /*is_component_ad=*/false, ad_descriptor);
    if (!matching_ad) {
      // Bid render url must match the interest group.
      return nullptr;
    }
    // Reporting IDs used by the server (if any) must match the ad on device.
    if (buyer_and_seller_reporting_id &&
        matching_ad->buyer_and_seller_reporting_id !=
            *buyer_and_seller_reporting_id) {
      return nullptr;
    }
    if (buyer_reporting_id &&
        matching_ad->buyer_reporting_id != *buyer_reporting_id) {
      return nullptr;
    }

    for (const auto& ad_component_descriptor : ad_component_descriptors) {
      const blink::InterestGroup::Ad* matching_ad_component = nullptr;
      if (interest_group.ad_components.has_value()) {
        matching_ad_component = FindMatchingAd(
            *interest_group.ad_components, bid_state->kanon_keys,
            interest_group, bid_role,
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*is_component_ad=*/true, ad_component_descriptor);
      }
      if (!matching_ad_component) {
        // Bid ad components must match the interest group.
        return nullptr;
      }
    }

    // 2. Reporting URLs must be okay
    // TODO(crbug.com/40273798): Implement reporting

    // 3. Private aggregation reporting requests.
    bid_state->private_aggregation_requests =
        std::move(component_win_pagg_requests);
    bid_state->server_filtered_pagg_requests_reserved =
        std::move(server_filtered_pagg_requests_reserved);
    bid_state->server_filtered_pagg_requests_non_reserved =
        std::move(server_filtered_pagg_requests_non_reserved);

    // 4. forDebuggingOnly reports.
    for (auto& [key, maybeReportUrl] : component_win_debugging_only_reports) {
      // From component auction. So cannot be top level seller debug reports.
      if (key.is_seller_report) {
        if (key.is_win_report) {
          bid_state->seller_debug_win_report_url = maybeReportUrl;
        } else {
          bid_state->seller_debug_loss_report_url = maybeReportUrl;
        }
      } else {
        if (key.is_win_report) {
          bid_state->bidder_debug_win_report_url = maybeReportUrl;
        } else {
          bid_state->bidder_debug_loss_report_url = maybeReportUrl;
        }
      }
    }

    bid_state->server_filtered_debugging_only_reports =
        std::move(server_filtered_debugging_only_reports);

    return std::make_unique<Bid>(
        bid_role, ad_metadata.value_or("null"), bid, bid_currency,
        /*ad_cost=*/std::nullopt, std::move(ad_descriptor),
        std::move(ad_component_descriptors),
        /*modeling_signals=*/std::nullopt,
        /*bid_duration=*/base::Seconds(0),
        /*bidding_signals_data_version=*/std::nullopt, matching_ad,
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt, bid_state,
        auction_);
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
      if (bid_states_[i]->trace_id) {
        TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("fledge", "bid_exceeds_size_limit",
                                            *bid_states_[i]->trace_id);
      }
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
      auction_->MaybeAddScriptFailureRealTimeContribution(
          /*is_buyer=*/true, bid_state->bidder->interest_group.owner);
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
        /*mojo_bids=*/{},
        /*bidding_signals_data_version=*/std::nullopt,
        /*debug_loss_report_url=*/std::nullopt,
        /*debug_win_report_url=*/std::nullopt,
        /*set_priority=*/std::nullopt,
        /*update_priority_signals_overrides=*/{},
        /*pa_requests=*/{},
        /*non_kanon_pa_requests=*/{},
        /*real_time_contributions=*/{},
        /*reject_reason=*/auction_worklet::mojom::RejectReason::kNotAvailable,
        /*errors=*/{});
  }

  base::flat_set<std::string> ComputeKAnon(
      const SingleStorageInterestGroup& storage_interest_group,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode) {
    if (kanon_mode == auction_worklet::mojom::KAnonymityBidMode::kNone) {
      return {};
    }

    // k-anon cache is always checked against the same time, to avoid weird
    // behavior of validity changing in the middle of the auction.
    base::Time start_time = auction_->auction_start_time_;
    if (IsKAnonDataExpired(storage_interest_group->last_k_anon_updated,
                           start_time)) {
      return {};
    }
    return storage_interest_group->hashed_kanon_keys;
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
    bid_state->kanon_keys = ComputeKAnon(bid_state->bidder, kanon_mode);
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
            interest_group.trusted_bidding_signals_slot_size_mode,
            interest_group.enable_bidding_signals_prioritization,
            interest_group.priority_vector, interest_group.execution_mode,
            interest_group.update_url,
            interest_group.trusted_bidding_signals_keys,
            interest_group.max_trusted_bidding_signals_url_length,
            interest_group.user_bidding_signals, interest_group.ads,
            interest_group.ad_components,
            KAnonKeysToMojom(bid_state->kanon_keys)),
        kanon_mode, bid_state->bidder->joining_origin,
        GetDirectFromSellerPerBuyerSignals(url_builder, owner_),
        GetDirectFromSellerAuctionSignals(url_builder),
        auction_->config_->seller,
        auction_->parent_ ? auction_->parent_->config_->seller
                          : std::optional<url::Origin>(),
        (base::Time::Now() - bid_state->bidder->join_time)
            .RoundToMultiple(base::Milliseconds(100)),
        bid_state->bidder->bidding_browser_signals.Clone(),
        auction_->auction_start_time_, auction_->RequestedAdSize(),
        multi_bid_limit_, *bid_state->trace_id, std::move(pending_remote),
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
    if (!auction_->config_promises_resolved_ ||
        auction_->direct_from_seller_signals_header_ad_slot_pending_) {
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
        GetDirectFromSellerPerBuyerSignalsHeaderAdSlot(
            *auction_->direct_from_seller_signals_header_ad_slot(),
            bid_state->bidder->interest_group.owner),
        GetDirectFromSellerAuctionSignals(url_builder),
        GetDirectFromSellerAuctionSignalsHeaderAdSlot(
            *auction_->direct_from_seller_signals_header_ad_slot()));
    bid_state->bid_finalizer.reset();
  }

  // Invoked when OnBiddingSignalsReceived() has been called for `state`, or
  // with a negative priority when the worklet process has an error, or the
  // buyer reaches their cumulative timeout, and is still waiting on the
  // OnBiddingSignalsReceived() invocation.
  void OnBiddingSignalsReceivedInternal(
      BidState* state,
      std::optional<double> new_priority,
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
      if (state->trace_id) {
        TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("fledge", "bid_filtered",
                                            *state->trace_id);
      }
      // Record if there are other bidders, as if there are not, the next call
      // may delete `this`.
      bool other_bidders = (num_outstanding_bids_ > 1);

      // If the result of applying the filter is negative, complete the bid
      // with OnGenerateBidCompleteInternal(), which will close the relevant
      // pipes and abort bid generation.
      OnGenerateBidCompleteInternal(
          state, /*mojo_bids=*/{},
          /*bidding_signals_data_version=*/std::nullopt,
          /*debug_loss_report_url=*/std::nullopt,
          /*debug_win_report_url=*/std::nullopt,
          /*set_priority=*/std::nullopt,
          /*update_priority_signals_overrides=*/{},
          /*pa_requests=*/{},
          /*non_kanon_pa_requests=*/{},
          /*real_time_contributions=*/{},
          /*reject_reason=*/auction_worklet::mojom::RejectReason::kNotAvailable,
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
    // repeatedly searching for bid states that had negative priority vector
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
        size_limit_ = std::min(size_limit_, bid_states_.size());
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
      std::vector<auction_worklet::mojom::BidderWorkletBidPtr> mojo_bids,
      std::optional<uint32_t> bidding_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url,
      std::optional<double> set_priority,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>
          update_priority_signals_overrides,
      PrivateAggregationRequests pa_requests,
      PrivateAggregationRequests non_kanon_pa_requests,
      RealTimeReportingContributions real_time_contributions,
      auction_worklet::mojom::RejectReason reject_reason,
      const std::vector<std::string>& errors) {
    DCHECK(!state->made_bid);
    DCHECK_GT(num_outstanding_bids_, 0);

    // We may not have a trace ID if we timed out before being delivered a
    // worklet.
    if (state->trace_id.has_value()) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidder_worklet_generate_bid",
                                      *state->trace_id);
    }

    const blink::InterestGroup& interest_group = state->bidder->interest_group;

    if (set_priority.has_value()) {
      auction_->interest_group_manager_->SetInterestGroupPriority(
          blink::InterestGroupKey(interest_group.owner, interest_group.name),
          set_priority.value());
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

    if (!ValidateBidderPrivateAggregationRequests(
            generate_bid_client_receiver_set_, pa_requests,
            non_kanon_pa_requests)) {
      mojo_bids.clear();
      pa_requests.clear();
      non_kanon_pa_requests.clear();
    }

    auction_->MaybeLogPrivateAggregationWebFeatures(pa_requests);
    if (!pa_requests.empty()) {
      PrivateAggregationPhaseKey agg_key = {
          interest_group.owner, PrivateAggregationPhase::kBidder,
          interest_group.aggregation_coordinator_origin};
      PrivateAggregationRequests& pa_requests_for_bidder =
          state->private_aggregation_requests[std::move(agg_key)];
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

    if (base::FeatureList::IsEnabled(
            blink::features::kFledgeRealTimeReporting) &&
        !base::FeatureList::IsEnabled(
            features::kCookieDeprecationFacilitatedTesting)) {
      if (!base::ranges::all_of(real_time_contributions,
                                HasValidRealTimeBucket)) {
        mojo_bids.clear();
        real_time_contributions.clear();
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Invalid real time reporting bucket");
      } else if (!base::ranges::all_of(real_time_contributions,
                                       HasValidRealTimePriorityWeight)) {
        mojo_bids.clear();
        real_time_contributions.clear();
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Invalid real time reporting priority weight");
      } else if (auction_->IsBuyerOptedInToRealTimeReporting(
                     interest_group.owner)) {
        // Only keep real time reporting contributions when the buyer is
        // opted-in.
        // Note that this adds an entry to the map for owner with an empty
        // vector as value, if owner was not in the map. This is important to
        // indicate that the owner is opted-in.
        RealTimeReportingContributions& real_time_contributions_for_origin =
            state->real_time_contributions[interest_group.owner];

        if (!real_time_contributions.empty()) {
          real_time_contributions_for_origin.insert(
              real_time_contributions_for_origin.end(),
              std::move_iterator(real_time_contributions.begin()),
              std::move_iterator(real_time_contributions.end()));
        }
      }
    }

    auction_->errors_.insert(auction_->errors_.end(), errors.begin(),
                             errors.end());

    // Convert from mojo to our state, and throw away the bids if any validation
    // fails.
    std::vector<std::unique_ptr<Bid>> bids;

    // Validate that we did not get too many bids and that the k-anon roles
    // aren't weird.
    auto kanon_mode = auction_->kanon_mode_;
    if (!IsValidMultiBid(kanon_mode, auction_->auction_metrics_recorder_,
                         mojo_bids, multi_bid_limit_)) {
      mojo_bids.clear();
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Too many bids or wrong roles");
    }

    if (reject_reason != auction_worklet::mojom::RejectReason::kNotAvailable &&
        reject_reason !=
            auction_worklet::mojom::RejectReason::kWrongGenerateBidCurrency &&
        reject_reason !=
            auction_worklet::mojom::RejectReason::kMultiBidLimitExceeded) {
      // Only possible reject reasons from generateBid(), besides the default
      // "not available", are "wrong generate bid currency" and "multi-bid
      // limit exceeded",
      mojo_bids.clear();
      state->reject_reason =
          auction_worklet::mojom::RejectReason::kNotAvailable;
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid bid reject_reason");
    } else {
      state->reject_reason = reject_reason;
    }

    // Provide legacy metrics for non-multibid cases.
    if (mojo_bids.empty()) {
      auction_->auction_metrics_recorder_->RecordInterestGroupWithNoBids();
    } else if (mojo_bids.size() == 1) {
      if (mojo_bids[0]->bid_role ==
          auction_worklet::mojom::BidRole::kBothKAnonModes) {
        auction_->auction_metrics_recorder_
            ->RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
      } else {
        auction_->auction_metrics_recorder_
            ->RecordInterestGroupWithOnlyNonKAnonBid();
      }
    } else if (mojo_bids.size() == 2 &&
               mojo_bids[0]->bid_role ==
                   auction_worklet::mojom::BidRole::kUnenforcedKAnon &&
               mojo_bids[1]->bid_role ==
                   auction_worklet::mojom::BidRole::kEnforcedKAnon) {
      auction_->auction_metrics_recorder_
          ->RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();
    } else {
      auction_->auction_metrics_recorder_
          ->RecordInterestGroupWithOtherMultiBid();
    }

    // Even bidders who do not bid are allowed to get loss reports.
    state->bidder_debug_loss_report_url = debug_loss_report_url;

    // `mojo_bids` is empty if the worklet doesn't bid, or if the bidder worklet
    // fails to load / crashes.
    for (auto& mojo_bid : mojo_bids) {
      auto bid = TryToCreateBid(std::move(mojo_bid), *state,
                                bidding_signals_data_version,
                                debug_loss_report_url, debug_win_report_url);
      if (bid) {
        bids.push_back(std::move(bid));
      } else {
        // Toss all the bids and don't do loss reporting for a compromised
        // worklet.
        bids.clear();
        state->bidder_debug_loss_report_url.reset();
        break;
      }
    }

    // Release the worklet. If it wins the auction, it will be requested again
    // to invoke its ReportWin() method.
    CloseBidStatePipes(*state);

    // End tracing for bid portion of the work if it started; it may not have
    // if the worklet failed before getting to `OnBidderWorkletReceived()`.
    if (state->trace_id.has_value()) {
      state->EndTracing();
    }

    if (!bids.empty()) {
      state->bidder_debug_win_report_url = debug_win_report_url;
      state->made_bid = true;
      for (auto& bid : bids) {
        auction_->ScoreBidIfReady(std::move(bid));
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

      auction_->OnScoringDependencyDone();
    }
  }

  void MaybeStartCumulativeTimeoutTimer() {
    // This should only be called when there are outstanding bids.
    DCHECK_GT(num_outstanding_bids_, 0);

    // Do nothing if still waiting on the seller to provide more of the
    // AuctionConfig, or directFromSellerSignalsHeaderAdSlot is still
    // parsing and finding a matching ad slot response, or waiting on a process
    // to be assigned (which would mean that this may be waiting behind other
    // buyers).
    if (!auction_->config_promises_resolved_ ||
        auction_->direct_from_seller_signals_header_ad_slot_pending_ ||
        !bidder_process_received_) {
      return;
    }

    DCHECK(!cumulative_buyer_timeout_timer_.IsRunning());

    // Get cumulative buyer timeout. Note that this must be done after the
    // `config_promises_resolved_` check above.
    cumulative_buyer_timeout_ =
        PerBuyerCumulativeTimeout(owner_, *auction_->config_);

    // Nothing to do if there's no cumulative timeout.
    if (!cumulative_buyer_timeout_) {
      return;
    }
    start_measuring_cumulative_time_ = base::TimeTicks::Now();

    cumulative_buyer_timeout_timer_.Start(
        FROM_HERE, *cumulative_buyer_timeout_,
        base::BindOnce(&BuyerHelper::OnTimeout, base::Unretained(this)));
  }

  // Called when the `cumulative_buyer_timeout_timer_` expires.
  void OnTimeout() {
    // If there are no outstanding bids, then the timer should not still be
    // running.
    DCHECK_GT(num_outstanding_bids_, 0);

    cumulative_buyer_timeout_triggered_ = true;

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
    num_bids_affected_by_cumulative_timeout_ = pending_bids.size();
    for (auto* pending_bid : pending_bids) {
      pending_bid->affected_by_cumulative_timeout = true;

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
      auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
      BidState& bid_state,
      const std::optional<uint32_t>& bidding_signals_data_version,
      const std::optional<GURL>& debug_loss_report_url,
      const std::optional<GURL>& debug_win_report_url) {
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
        *interest_group.ads, bid_state.kanon_keys, interest_group,
        mojo_bid->bid_role, mojo_bid->selected_buyer_and_seller_reporting_id,
        /*is_component_ad=*/false, mojo_bid->ad_descriptor);
    if (!matching_ad) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Bid render ad must have a valid URL and size (if specified)");
      return nullptr;
    }
    // If the matching ad does not have size specified, the bid to be created
    // should not have size specified to fall back to old behavior.
    blink::AdDescriptor ad_descriptor(
        mojo_bid->ad_descriptor.url,
        matching_ad->size_group ? mojo_bid->ad_descriptor.size : std::nullopt);

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
          blink::MaxAdAuctionAdComponents()) {
        generate_bid_client_receiver_set_.ReportBadMessage(
            "Too many ad component URLs");
        return nullptr;
      }

      // Validate each ad component URL is valid and appears in the interest
      // group's `ad_components` field.
      for (const blink::AdDescriptor& ad_component_descriptor :
           *mojo_bid->ad_component_descriptors) {
        const blink::InterestGroup::Ad* matching_ad_component = FindMatchingAd(
            *interest_group.ad_components, bid_state.kanon_keys, interest_group,
            mojo_bid->bid_role,
            /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
            /*is_component_ad=*/true, ad_component_descriptor);
        if (!matching_ad_component) {
          generate_bid_client_receiver_set_.ReportBadMessage(
              "Bid ad component must have a valid URL and size (if specified)");
          return nullptr;
        }
        // If the matching ad does not have size specified, the bid to be
        // created should not have size specified to fall back to old behavior.
        ad_component_descriptors.emplace_back(ad_component_descriptor.url,
                                              matching_ad_component->size_group
                                                  ? ad_component_descriptor.size
                                                  : std::nullopt);
      }
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

    if (mojo_bid->selected_buyer_and_seller_reporting_id.has_value() &&
        !IsSelectedReportingIdValid(
            matching_ad->selectable_buyer_and_seller_reporting_ids,
            *mojo_bid->selected_buyer_and_seller_reporting_id)) {
      generate_bid_client_receiver_set_.ReportBadMessage(
          "Invalid selected buyer and seller reporting id");
      return nullptr;
    }

    return std::make_unique<Bid>(
        mojo_bid->bid_role, std::move(mojo_bid->ad), mojo_bid->bid,
        std::move(mojo_bid->bid_currency), mojo_bid->ad_cost,
        std::move(ad_descriptor), std::move(ad_component_descriptors),
        std::move(mojo_bid->modeling_signals), mojo_bid->bid_duration,
        bidding_signals_data_version, matching_ad,
        std::move(mojo_bid->selected_buyer_and_seller_reporting_id), &bid_state,
        auction_);
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

  // How many bids generateBid() is permitted to return at once.
  const uint16_t multi_bid_limit_;

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
  // Starts once `bidder_process_received_`,
  // `auction_->config_promises_resolved_` are true, and
  // `auction_->direct_from_seller_signals_header_ad_slot_pending_` is false,
  // if `cumulative_buyer_timeout_` is not nullopt.
  base::OneShotTimer cumulative_buyer_timeout_timer_;

  int num_outstanding_bidding_signals_received_calls_ = 0;
  int num_outstanding_bids_ = 0;

  // How many IGs had their execution cancelled by cumulative timeout.
  size_t num_bids_affected_by_cumulative_timeout_ = 0;

  // Records the time at which StartGeneratingBids was called for UKM.
  base::TimeTicks start_generating_bids_time_;

  // Per-buyer PA metrics.
  PrivateAggregationParticipantData buyer_metrics_;
  AuctionMetricsRecorder::LatencyAggregator code_fetch_time_;
  int bidder_scripts_timed_out_ = 0;
  base::TimeTicks start_measuring_cumulative_time_;
  base::TimeTicks stop_measuring_cumulative_time_;
  std::optional<base::TimeDelta> cumulative_buyer_timeout_;
  bool cumulative_buyer_timeout_triggered_ = false;

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
    AuctionMetricsRecorder* auction_metrics_recorder,
    AuctionWorkletManager* auction_worklet_manager,
    AuctionNonceManager* auction_nonce_manager,
    InterestGroupManagerImpl* interest_group_manager,
    GetDataDecoderCallback get_data_decoder_callback,
    base::Time auction_start_time,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    base::RepeatingCallback<
        void(const PrivateAggregationRequests& private_aggregation_requests)>
        maybe_log_private_aggregation_web_features_callback)
    : devtools_auction_id_(base::Token::CreateRandom().ToString()),
      trace_id_(base::trace_event::GetNextGlobalTraceId()),
      kanon_mode_(kanon_mode),
      auction_metrics_recorder_(auction_metrics_recorder),
      auction_worklet_manager_(auction_worklet_manager),
      auction_nonce_manager_(auction_nonce_manager),
      interest_group_manager_(interest_group_manager),
      config_(config),
      config_promises_resolved_(config_->NumPromises() == 0),
      parent_(parent),
      negative_targeter_(std::make_unique<AdAuctionNegativeTargeter>()),
      auction_start_time_(auction_start_time),
      creation_time_(base::TimeTicks::Now()),
      is_interest_group_api_allowed_callback_(
          std::move(is_interest_group_api_allowed_callback)),
      maybe_log_private_aggregation_web_features_callback_(
          std::move(maybe_log_private_aggregation_web_features_callback)),
      is_server_auction_(config->server_response.has_value()),
      get_data_decoder_callback_(std::move(get_data_decoder_callback)) {
  DCHECK(is_interest_group_api_allowed_callback_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("fledge", "auction", *trace_id_,
                                    "decision_logic_url",
                                    config_->decision_logic_url);

  // Warm up decoder.
  get_data_decoder_callback_.Run(config->seller);

  FrameTreeNodeId frame_tree_node_id =
      auction_worklet_manager_->GetFrameTreeNodeID();
  if (devtools_instrumentation::NeedInterestGroupAuctionEvents(
          frame_tree_node_id)) {
    devtools_instrumentation::OnInterestGroupAuctionEventOccurred(
        frame_tree_node_id, auction_start_time_,
        InterestGroupAuctionEventType::kStarted, devtools_auction_id_,
        parent_ ? base::optional_ref<const std::string>(
                      parent->devtools_auction_id_)
                : base::optional_ref<const std::string>(),
        SerializeAuctionConfigForDevtools(*config_));
  }

  uint32_t child_pos = 0;
  for (const auto& component_auction_config :
       config->non_shared_params.component_auctions) {
    // Nested component auctions are not supported.
    DCHECK(!parent_);
    // Top-level server auctions are not supported.
    DCHECK(!is_server_auction_);
    component_auctions_.emplace(
        child_pos, std::make_unique<InterestGroupAuction>(
                       kanon_mode_, &component_auction_config, /*parent=*/this,
                       auction_metrics_recorder_, auction_worklet_manager,
                       auction_nonce_manager, interest_group_manager,
                       get_data_decoder_callback_, auction_start_time_,
                       is_interest_group_api_allowed_callback_,
                       maybe_log_private_aggregation_web_features_callback_));
    ++child_pos;
  }

  if (!parent_) {
    auction_metrics_recorder_->SetKAnonymityBidMode(kanon_mode);
    auction_metrics_recorder_->SetNumConfigPromises(config_->NumPromises());
  }
}

InterestGroupAuction::~InterestGroupAuction() {
  if (trace_id_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "auction", *trace_id_);
  }

  if (!final_auction_result_) {
    final_auction_result_ = AuctionResult::kAborted;
  }

  std::string uma_prefix = "Ads.InterestGroup.Auction.";
  if (is_server_auction_) {
    uma_prefix = "Ads.InterestGroup.ServerAuction.";
  }

  // TODO(mmenke): Record histograms for component auctions.
  if (!parent_) {
    base::UmaHistogramEnumeration(uma_prefix + "Result",
                                  *final_auction_result_);

    if (HasNonKAnonWinner()) {
      base::UmaHistogramBoolean(uma_prefix + "NonKAnonWinnerIsKAnon",
                                NonKAnonWinnerIsKAnon());
    }

    // Only record time of full auctions and aborts.
    base::TimeTicks now = base::TimeTicks::Now();
    switch (*final_auction_result_) {
      case AuctionResult::kAborted:
        base::UmaHistogramMediumTimes(uma_prefix + "AbortTime",
                                      now - creation_time_);
        break;
      case AuctionResult::kNoBids:
      case AuctionResult::kAllBidsRejected:
        base::UmaHistogramMediumTimes(uma_prefix + "CompletedWithoutWinnerTime",
                                      now - creation_time_);
        if (is_server_auction_) {
          base::UmaHistogramMediumTimes(
              "Ads.InterestGroup.ServerAuction.EndToEndTimeNoWinner",
              now - get_ad_auction_data_start_time_);
        }
        break;
      case AuctionResult::kSuccess:
        base::UmaHistogramMediumTimes(uma_prefix + "AuctionWithWinnerTime",
                                      now - creation_time_);
        if (is_server_auction_) {
          base::UmaHistogramMediumTimes(
              "Ads.InterestGroup.ServerAuction.EndToEndTime",
              now - get_ad_auction_data_start_time_);
        }
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
    AuctionPhaseCompletionCallback load_interest_groups_phase_callback) {
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
  if (!is_interest_group_api_allowed_callback_.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          config_->seller) ||
      BlockDueToDisallowedCrossOriginTrustedSellerSignals()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
            weak_ptr_factory_.GetWeakPtr(), AuctionResult::kSellerRejected));
    return;
  }

  if (config_->non_shared_params.auction_nonce) {
    if (!auction_nonce_manager_->ClaimAuctionNonceIfAvailable(
            static_cast<AuctionNonce>(
                *config_->non_shared_params.auction_nonce))) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
              weak_ptr_factory_.GetWeakPtr(),
              AuctionResult::kInvalidAuctionNonce));
      return;
    }
  }

  for (auto component_auction = component_auctions_.begin();
       component_auction != component_auctions_.end(); ++component_auction) {
    if (component_auction->second->is_server_auction_) {
      // Don't load interest groups here for server auctions.
      continue;
    }
    component_auction->second->StartLoadInterestGroupsPhase(
        base::BindOnce(&InterestGroupAuction::OnComponentInterestGroupsRead,
                       weak_ptr_factory_.GetWeakPtr(), component_auction));
    ++num_pending_loads_;
  }

  if (config_->non_shared_params.interest_group_buyers) {
    for (const auto& buyer :
         *config_->non_shared_params.interest_group_buyers) {
      if (!is_interest_group_api_allowed_callback_.Run(
              ContentBrowserClient::InterestGroupApiOperation::kBuy, buyer)) {
        continue;
      }
      interest_group_manager_->GetInterestGroupsForOwner(
          devtools_auction_id_, buyer,
          base::BindOnce(&InterestGroupAuction::OnInterestGroupRead,
                         weak_ptr_factory_.GetWeakPtr()));
      ++num_pending_loads_;
    }
  }

  if (num_pending_loads_ == 0) {
    // There is nothing to load.  We move on to the bidding and scoring phase
    // anyway, since it may need to wait for config promises to be resolved
    // (and checked) and also potentially deal with additional_bids.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete,
            weak_ptr_factory_.GetWeakPtr(), AuctionResult::kSuccess));
  }
}

void InterestGroupAuction::StartBiddingAndScoringPhase(
    std::optional<DebugReportLockoutAndCooldowns>
        debug_report_lockout_and_cooldowns,
    base::OnceClosure on_seller_receiver_callback,
    AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback) {
  DCHECK(bidding_and_scoring_phase_callback);
  DCHECK(!on_seller_receiver_callback_);
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK(!non_kanon_enforced_auction_leader_.top_bid);
  DCHECK(!kanon_enforced_auction_leader_.top_bid);
  DCHECK_EQ(pending_component_seller_worklet_requests_, 0u);
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kBefore);

  bidding_and_scoring_phase_state_ = PhaseState::kDuring;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "bidding_and_scoring_phase",
                                    *trace_id_);

  if (debug_report_lockout_and_cooldowns.has_value()) {
    debug_report_lockout_and_cooldowns_ = *debug_report_lockout_and_cooldowns;
  }

  on_seller_receiver_callback_ = std::move(on_seller_receiver_callback);
  bidding_and_scoring_phase_callback_ =
      std::move(bidding_and_scoring_phase_callback);

  bidding_and_scoring_phase_start_time_ = base::TimeTicks::Now();

  CHECK_EQ(num_scoring_dependencies_, 0);
  num_scoring_dependencies_ =
      buyer_helpers_.size() + component_auctions_.size();

  // Also wait for config to resolve.
  if (!config_promises_resolved_) {
    ++num_scoring_dependencies_;
  }

  // Also wait for directFromSellerSignalsHeaderAdSlot to finish JSON parsing.
  if (direct_from_seller_signals_header_ad_slot_pending_) {
    ++num_scoring_dependencies_;
  }

  DecodeAdditionalBidsIfReady();

  // Need to start loading worklets before any bids can be generated or scored.
  if (component_auctions_.empty()) {
    if (is_server_auction_) {
      if (saved_response_) {
        MaybeLoadDebugReportLockoutAndCooldowns();
        return;
      }
    } else {
      // If there are no component auctions, request the seller worklet if we
      // may need it. (The case for component auctions is handled below, there
      // the seller worklet will be requested once all component auctions have
      // received their own seller worklets).
      if (!buyer_helpers_.empty() || MayHaveAdditionalBids()) {
        RequestSellerWorklet();
      }
    }
  } else {
    // Since component auctions may invoke OnComponentSellerWorkletReceived()
    // synchronously, it's important to set this to the total number of
    // component auctions before invoking StartBiddingAndScoringPhase() on any
    // component auction.
    size_t local_auctions = 0;
    for (const auto& component_auction_info : component_auctions_) {
      if (!component_auction_info.second->is_server_auction_) {
        local_auctions++;
      }
    }

    pending_component_seller_worklet_requests_ = local_auctions;
    for (auto& component_auction_info : component_auctions_) {
      InterestGroupAuction* component_auction =
          component_auction_info.second.get();
      base::OnceClosure component_on_seller_receiver_callback =
          component_auction->is_server_auction_
              ? base::NullCallback()
              : base::BindOnce(
                    &InterestGroupAuction::OnComponentSellerWorkletReceived,
                    base::Unretained(this));
      component_auction->StartBiddingAndScoringPhase(
          debug_report_lockout_and_cooldowns.has_value()
              ? std::optional<DebugReportLockoutAndCooldowns>(
                    *debug_report_lockout_and_cooldowns)
              : std::nullopt,
          std::move(component_on_seller_receiver_callback),
          base::BindOnce(&InterestGroupAuction::OnComponentAuctionComplete,
                         base::Unretained(this), component_auction));
    }
    // If there are no local auctions then we need to request the top-level
    // seller worklet. In the case where there are any local auctions this will
    // happen when all local component auctions complete.
    if (local_auctions == 0) {
      RequestSellerWorklet();
    }
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    buyer_helper->SetForDebuggingOnlyInCooldownOrLockout(
        IsOriginInDebugReportCooldownOrLockout(
            buyer_helper->owner(), debug_report_lockout_and_cooldowns_,
            base::Time::Now()));
    buyer_helper->StartGeneratingBids();
  }

  // It's possible that we actually have nothing to do here at this point ---
  // neither bids from interest groups, nor configuration promises to wait for.
  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::HandleComponentServerResponse(
    uint32_t pos,
    mojo_base::BigBuffer response,
    AdAuctionPageData& ad_auction_page_data) {
  CHECK(!parent_);  // Should not be called on a component.
  auto it = component_auctions_.find(pos);

  if (it == component_auctions_.end()) {
    // Empty component auctions shouldn't be dropped, but component
    // auctions may still be dropped if they fail permissions checks.
    return;
  }

  InterestGroupAuction* component_auction = it->second.get();
  component_auction->HandleServerResponse(std::move(response),
                                          ad_auction_page_data);
}

void InterestGroupAuction::HandleServerResponse(
    mojo_base::BigBuffer response,
    AdAuctionPageData& ad_auction_page_data) {
  if (!HandleServerResponseImpl(std::move(response), ad_auction_page_data)) {
    DCHECK(saved_response_);
    if (bidding_and_scoring_phase_state_ == PhaseState::kDuring) {
      MaybeCompleteBiddingAndScoringPhase();
    }
  }
}

bool InterestGroupAuction::HandleServerResponseImpl(
    mojo_base::BigBuffer response,
    AdAuctionPageData& ad_auction_page_data) {
  // Check that response was witnessed by seller origin.
  std::array<uint8_t, crypto::kSHA256Length> hash =
      crypto::SHA256Hash(response);
  if (!ad_auction_page_data.WitnessedAuctionResultForOrigin(
          config_->seller,
          std::string(reinterpret_cast<char*>(hash.data()), hash.size()))) {
    // If it wasn't witnessed then we don't know that it came from the server.
    saved_response_.emplace();
    base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                  InvalidServerResponseReason::kNotWitnessed);
    errors_.push_back(
        base::StrCat({"runAdAuction(): Server response was not witnessed from ",
                      config_->seller.Serialize()}));
    return false;
  }

  AdAuctionRequestContext* request_context =
      ad_auction_page_data.GetContextForAdAuctionRequest(
          config_->server_response->request_id);
  if (!request_context) {
    // The corresponding context for the requested blob couldn't be found.
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kUnknownRequestId);
    errors_.push_back(base::StrCat(
        {"runAdAuction(): No corresponding request with ID: ",
         config_->server_response->request_id.AsLowercaseString()}));
    return false;
  }

  // The auction must be for the same seller that requested the blob.
  if (request_context->seller != config_->seller) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                  InvalidServerResponseReason::kSellerMismatch);
    errors_.push_back(
        "runAdAuction(): Seller in response doesn't match request");
    return false;
  }
  get_ad_auction_data_start_time_ = request_context->start_time;

  // Trigger updates for buyers in the auction config.
  if (config_->non_shared_params.interest_group_buyers.has_value()) {
    const std::vector<url::Origin>& buyers =
        config_->non_shared_params.interest_group_buyers.value();
    post_auction_update_owners_.insert(post_auction_update_owners_.end(),
                                       buyers.begin(), buyers.end());
  }

  auto maybe_response =
      quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
          std::string(reinterpret_cast<char*>(response.data()),
                      response.size()),
          request_context->context,
          base::FeatureList::IsEnabled(kBiddingAndAuctionEncryptionMediaType)
              ? kBiddingAndAuctionEncryptionResponseMediaType
              : quiche::ObliviousHttpHeaderKeyConfig::kOhttpResponseLabel);
  if (!maybe_response.ok()) {
    // We couldn't decrypt the response.
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kDecryptionFailure);
    errors_.push_back("runAdAuction(): Could not decrypt server response");
    return false;
  }
  const std::string& plaintext_response = maybe_response->GetPlaintextData();
  std::optional<base::span<const uint8_t>> compressed_response =
      ExtractCompressedBiddingAndAuctionResponse(
          base::as_bytes(base::make_span(plaintext_response)));
  if (!compressed_response) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kUnframingFailure);
    errors_.push_back("runAdAuction(): Could not parse response framing");
    return false;
  }

  data_decoder::DataDecoder* data_decoder =
      get_data_decoder_callback_.Run(config_->seller);
  if (!data_decoder) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kDecoderShutdown);
    errors_.push_back("runAdAuction(): Page in shutdown");
    return false;
  }

  data_decoder->GzipUncompress(
      std::move(compressed_response).value(),
      base::BindOnce(
          &InterestGroupAuction::OnDecompressedServerResponse,
          // This use of unretained is safe since the request_context is owned
          // by a PageUserData and is only deleted when the page is destroyed.
          // This auction is owned by the page through a DocumentService, so it
          // will be destroyed before the decoder.
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(request_context)));
  return true;
}

std::unique_ptr<InterestGroupAuctionReporter>
InterestGroupAuction::CreateReporter(
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AdAuctionPageDataCallback ad_auction_page_data_callback,
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

  SubresourceUrlBuilderIfReady();

  const LeaderInfo& leader = leader_info();
  InterestGroupAuction::ScoredBid* winner = leader.top_bid.get();
  InterestGroupAuctionReporter::WinningBidInfo winning_bid_info(
      winner->bid->bid_state->bidder);
  winning_bid_info.render_url = winner->bid->ad_descriptor.url;
  winning_bid_info.ad_components = winner->bid->GetAdComponentUrls();
  winning_bid_info.allowed_reporting_origins =
      winner->bid->bid_ad->allowed_reporting_origins;
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
  // We redact the bid currency to just be the concrete currency from the
  // config; passing in the currency from actual bid would permit leakage of
  // information, if only via its absence.
  winning_bid_info.bid_currency = PerBuyerCurrency(
      bidder_bid->interest_group->owner, *bidder_bid->auction->config_);

  winning_bid_info.provided_as_additional_bid =
      bidder_bid->bid_state->additional_bid_buyer.has_value();
  winning_bid_info.ad_cost = bidder_bid->ad_cost;
  winning_bid_info.modeling_signals = bidder_bid->modeling_signals;
  winning_bid_info.bid_duration = winner->bid->bid_duration;
  winning_bid_info.bidding_signals_data_version =
      winner->bid->bidding_signals_data_version;
  winning_bid_info.selected_buyer_and_seller_reporting_id =
      winner->bid->selected_buyer_and_seller_reporting_id;
  base::Value::Dict ad_metadata;
  ad_metadata.Set("renderURL", winner->bid->ad_descriptor.url.spec());
  if (winner->bid->bid_ad->metadata) {
    ad_metadata.Set("metadata", winner->bid->bid_ad->metadata.value());
  }
  if (winner->bid->bid_ad->ad_render_id) {
    ad_metadata.Set("adRenderId", winner->bid->bid_ad->ad_render_id.value());
  }
  JSONStringValueSerializer serializer(&winning_bid_info.ad_metadata);
  serializer.Serialize(base::Value(std::move(ad_metadata)));

  InterestGroupAuctionReporter::SellerWinningBidInfo
      top_level_seller_winning_bid_info;
  top_level_seller_winning_bid_info.auction_config = config_;
  // Subresource URL builder must have been created by scoring or earlier in
  // this function.
  DCHECK(subresource_url_builder_);
  CHECK(direct_from_seller_signals_header_ad_slot_);  // Should never be null.
  top_level_seller_winning_bid_info.subresource_url_builder =
      std::move(subresource_url_builder_);
  top_level_seller_winning_bid_info.direct_from_seller_signals_header_ad_slot =
      std::move(direct_from_seller_signals_header_ad_slot_);
  top_level_seller_winning_bid_info.bid = winner->bid->bid;
  top_level_seller_winning_bid_info.rounded_bid =
      InterestGroupAuctionReporter::RoundBidStochastically(
          top_level_seller_winning_bid_info.bid);

  if (winner->bid->auction == this) {
    // Bid came directly from bidder, not a component auction.
    top_level_seller_winning_bid_info.bid_currency =
        winning_bid_info.bid_currency;
  } else {
    // Bid comes from component auction, so we redact the config expectation of
    // the bid of component auction in top-level auction.
    InterestGroupAuction* component_auction = winner->bid->auction;
    top_level_seller_winning_bid_info.bid_currency =
        component_auction->config_->non_shared_params.seller_currency;
    if (!top_level_seller_winning_bid_info.bid_currency) {
      top_level_seller_winning_bid_info.bid_currency =
          PerBuyerCurrency(component_auction->config_->seller, *config_);
    }
  }

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
  top_level_seller_winning_bid_info.saved_response = std::move(saved_response_);

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
  std::optional<InterestGroupAuctionReporter::SellerWinningBidInfo>
      component_seller_winning_bid_info;
  if (winner->bid->auction != this) {
    InterestGroupAuction* component_auction = winner->bid->auction;
    component_auction->SubresourceUrlBuilderIfReady();

    component_seller_winning_bid_info.emplace();
    component_seller_winning_bid_info->auction_config =
        component_auction->config_;
    DCHECK(component_auction->subresource_url_builder_);
    // Should never be null.
    CHECK(component_auction->direct_from_seller_signals_header_ad_slot_);
    component_seller_winning_bid_info->subresource_url_builder =
        std::move(component_auction->subresource_url_builder_);
    component_seller_winning_bid_info
        ->direct_from_seller_signals_header_ad_slot = std::move(
        component_auction->direct_from_seller_signals_header_ad_slot_);
    const LeaderInfo& component_leader = component_auction->leader_info();
    component_seller_winning_bid_info->bid = component_leader.top_bid->bid->bid;
    component_seller_winning_bid_info->rounded_bid =
        InterestGroupAuctionReporter::RoundBidStochastically(
            component_leader.top_bid->bid->bid);
    // The bidder in this auction was the actual bidder, so the currency comes
    // from it, too.
    component_seller_winning_bid_info->bid_currency =
        winning_bid_info.bid_currency;
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
    component_seller_winning_bid_info->saved_response =
        std::move(component_auction->saved_response_);
  }

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
      debug_win_report_urls, debug_loss_report_urls);

  bool bid_is_kanon;
  switch (kanon_mode_) {
    case auction_worklet::mojom::KAnonymityBidMode::kSimulate:
      // Check if the winner was k-anon, since we use the non-kanonymous winner
      // in simulate mode.
      bid_is_kanon = NonKAnonWinnerIsKAnon();
      break;
    case auction_worklet::mojom::KAnonymityBidMode::kEnforce:
      // We always have a winner when we get here.
      bid_is_kanon = true;
      break;
    case auction_worklet::mojom::KAnonymityBidMode::kNone:
      // Set to false due to enforcement is completely off.
      bid_is_kanon = false;
      break;
  }

  auto result = std::make_unique<InterestGroupAuctionReporter>(
      interest_group_manager_, auction_worklet_manager_, browser_context,
      private_aggregation_manager,
      maybe_log_private_aggregation_web_features_callback_,
      std::move(ad_auction_page_data_callback), std::move(auction_config),
      devtools_auction_id_, main_frame_origin, frame_origin,
      std::move(client_security_state), std::move(url_loader_factory),
      kanon_mode_, bid_is_kanon, std::move(winning_bid_info),
      std::move(top_level_seller_winning_bid_info),
      std::move(component_seller_winning_bid_info),
      std::move(interest_groups_that_bid), std::move(debug_win_report_urls),
      std::move(debug_loss_report_urls), GetKAnonKeysToJoin(),
      TakeReservedPrivateAggregationRequests(),
      TakeNonReservedPrivateAggregationRequests(),
      ComputePrivateAggregationParticipantData(),
      TakeRealTimeReportingContributions());

  // Avoid dangling pointers for things transferred to the reporter.
  winner->bid->interest_group = nullptr;
  winner->bid->bid_ad = nullptr;
  config_ = nullptr;
  for (const auto& kv : component_auctions_) {
    kv.second->config_ = nullptr;
  }

  return result;
}

void InterestGroupAuction::NotifyConfigPromisesResolved() {
  DCHECK(!config_promises_resolved_);
  DCHECK_EQ(0, config_->NumPromises());
  config_promises_resolved_ = true;

  auction_metrics_recorder_->OnConfigPromisesResolved();

  FrameTreeNodeId frame_tree_node_id =
      auction_worklet_manager_->GetFrameTreeNodeID();
  if (devtools_instrumentation::NeedInterestGroupAuctionEvents(
          frame_tree_node_id)) {
    devtools_instrumentation::OnInterestGroupAuctionEventOccurred(
        frame_tree_node_id, base::Time::Now(),
        InterestGroupAuctionEventType::kConfigResolved, devtools_auction_id_,
        parent_ ? base::optional_ref<const std::string>(
                      parent_->devtools_auction_id_)
                : base::optional_ref<const std::string>(),
        SerializeAuctionConfigForDevtools(*config_));
  }

  // If we haven't started the bidding and scoring phase, we will just handle
  // this information at its start; setting `config_promises_resolved_` is
  // enough both for us and the BuyerHelper. If we are after the phase, that
  // means that promises resolved after we failed out of it, so we should ignore
  // them.
  if (bidding_and_scoring_phase_state_ != PhaseState::kDuring) {
    return;
  }

  for (const auto& buyer_helper : buyer_helpers_) {
    buyer_helper->NotifyConfigPromisesResolved();
  }

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& unscored_bid : unscored_bids_) {
    unscored_bid->wait_promises =
        now - unscored_bid->trace_wait_seller_deps_start;

    if (!component_auctions_.empty()) {
      auction_metrics_recorder_
          ->RecordTopLevelBidQueuedWaitingForConfigPromises(
              unscored_bid->wait_promises);
    } else {
      auction_metrics_recorder_->RecordBidQueuedWaitingForConfigPromises(
          unscored_bid->wait_promises);
    }
  }

  DecodeAdditionalBidsIfReady();

  // Config resolution is done.
  OnScoringDependencyDone();

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

void InterestGroupAuction::NotifyAdditionalBidsConfig(
    AdAuctionPageData& auction_page_data) {
  // An auction with additional bids can't have child auctions.
  DCHECK_EQ(config_->non_shared_params.component_auctions.size(), 0u);

  // Enforced by mojo traits and checks on ResolvedAdditionalBids().
  CHECK(config_->non_shared_params.auction_nonce.has_value());
  CHECK(config_->non_shared_params.interest_group_buyers.has_value());

  interest_group_buyers_ = base::flat_set<url::Origin>(
      config_->non_shared_params.interest_group_buyers->begin(),
      config_->non_shared_params.interest_group_buyers->end());

  encoded_signed_additional_bids_ =
      auction_page_data.TakeAuctionAdditionalBidsForOriginAndNonce(
          config_->seller,
          config_->non_shared_params.auction_nonce->AsLowercaseString());
}

void InterestGroupAuction::NotifyComponentAdditionalBidsConfig(
    uint32_t pos,
    AdAuctionPageData& auction_page_data) {
  DCHECK(!parent_);  // Should not be called on a component.
  // And should have component auctions configured if we got thus far
  // (though none may have ended up passing the permissions check).
  DCHECK_GT(config_->non_shared_params.component_auctions.size(), 0u);

  auto it = component_auctions_.find(pos);
  if (it == component_auctions_.end()) {
    // Empty component auctions shouldn't be dropped, but component
    // auctions may still be dropped if they fail permissions checks.
    return;
  }

  it->second->NotifyAdditionalBidsConfig(auction_page_data);
}

void InterestGroupAuction::NotifyDirectFromSellerSignalsHeaderAdSlotConfig(
    AdAuctionPageData& auction_page_data,
    const std::optional<std::string>&
        direct_from_seller_signals_header_ad_slot) {
  CHECK(!direct_from_seller_signals_header_ad_slot_pending_);
  if (!direct_from_seller_signals_header_ad_slot ||
      bidding_and_scoring_phase_state_ == PhaseState::kAfter) {
    return;
  }
  if (bidding_and_scoring_phase_state_ == PhaseState::kDuring) {
    ++num_scoring_dependencies_;
  }
  direct_from_seller_signals_header_ad_slot_pending_ = true;
  auction_page_data.ParseAndFindAdAuctionSignals(
      config_->seller, *direct_from_seller_signals_header_ad_slot,
      base::BindOnce(
          &InterestGroupAuction::OnDirectFromSellerSignalHeaderAdSlotResolved,
          weak_ptr_factory_.GetWeakPtr(),
          *direct_from_seller_signals_header_ad_slot));
}

void InterestGroupAuction::
    NotifyComponentDirectFromSellerSignalsHeaderAdSlotConfig(
        uint32_t pos,
        AdAuctionPageData& auction_page_data,
        const std::optional<std::string>&
            direct_from_seller_signals_header_ad_slot) {
  CHECK(!parent_);  // Should not be called on a component.
  auto it = component_auctions_.find(pos);

  if (it == component_auctions_.end()) {
    // Empty component auctions shouldn't be dropped, but component
    // auctions may still be dropped if they fail permissions checks.
    return;
  }

  it->second->NotifyDirectFromSellerSignalsHeaderAdSlotConfig(
      auction_page_data, std::move(direct_from_seller_signals_header_ad_slot));
}

void InterestGroupAuction::ClosePipes() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  score_ad_receivers_.Clear();

  for (auto& buyer_helper : buyer_helpers_) {
    buyer_helper->ClosePipes();
  }
  seller_worklet_handle_.reset();

  // There shouldn't be any pipes here for additional bids (though reporter
  // may create some).
  for (const auto& bid_state : bid_states_for_additional_bids_) {
    DCHECK(!bid_state->worklet_handle);
    DCHECK(!bid_state->bid_finalizer);
  }

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

  if (saved_response_) {
    interest_groups.insert(saved_response_->bidding_groups.begin(),
                           saved_response_->bidding_groups.end());
    for (const auto& ig_bid : interest_groups) {
      interest_group_manager_->NotifyInterestGroupAccessed(
          devtools_auction_id_,
          InterestGroupManagerImpl::InterestGroupObserver::
              InterestGroupObserver::kBid,
          ig_bid.owner, ig_bid.name,
          /*component_seller_origin=*/std::nullopt,
          // Don't know individual bid values of a server-side auction, just
          // that the IGs participated.
          /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);
    }
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

std::optional<blink::AdSize> InterestGroupAuction::RequestedAdSize() const {
  return config_->non_shared_params.requested_size;
}

std::string_view GetRejectReasonString(
    const auction_worklet::mojom::RejectReason reject_reason) {
  std::string_view reject_reason_str;
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
    case auction_worklet::mojom::RejectReason::kWrongGenerateBidCurrency:
      reject_reason_str = "wrong-generate-bid-currency";
      break;
    case auction_worklet::mojom::RejectReason::kWrongScoreAdCurrency:
      reject_reason_str = "wrong-score-ad-currency";
      break;
    case auction_worklet::mojom::RejectReason::kMultiBidLimitExceeded:
      reject_reason_str = "multi-bid-limit-exceeded";
      break;
  }
  return reject_reason_str;
}

GURL InterestGroupAuction::FillPostAuctionSignals(
    const GURL& url,
    const PostAuctionSignals& signals,
    const std::optional<PostAuctionSignals>& top_level_signals,
    const std::optional<auction_worklet::mojom::RejectReason> reject_reason) {
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

  std::optional<absl::uint128> bucket_base =
      BucketBaseForReportPaBuyers(*config_, interest_group.owner);
  if (!bucket_base) {
    return false;
  }

  std::optional<
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>
      report_buyers_config =
          ReportBuyersConfigForPaBuyers(buyer_report_type, *config_);
  if (!report_buyers_config) {
    return false;
  }

  std::optional<
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyerDebugModeConfig>
      debug_mode_config;

  if (base::FeatureList::IsEnabled(
          blink::features::
              kPrivateAggregationAuctionReportBuyerDebugModeConfig)) {
    debug_mode_config =
        config_->non_shared_params.auction_report_buyer_debug_mode_config;
  }

  blink::mojom::DebugModeDetailsPtr debug_mode_details;
  if (debug_mode_config) {
    blink::mojom::DebugKeyPtr debug_key = nullptr;
    if (debug_mode_config->debug_key) {
      CHECK(debug_mode_config->is_enabled);
      debug_key = blink::mojom::DebugKey::New(*debug_mode_config->debug_key);
    }
    debug_mode_details = blink::mojom::DebugModeDetails::New(
        debug_mode_config->is_enabled, std::move(debug_key));
  } else {
    // If the debug mode is not provided, use the default (debug mode disabled).
    debug_mode_details = blink::mojom::DebugModeDetails::New();
  }

  // TODO(caraitto): Consider adding renderer and Mojo validation to ensure that
  // bucket sums can't be out of range, and scales can't be negative, infinite,
  // or NaN.
  // TODO(crbug.com/330744610): Consider allowing filtering ID to be set.
  PrivateAggregationKey agg_key = {config_->seller,
                                   config_->aggregation_coordinator_origin};
  PrivateAggregationRequests& destination_vector =
      private_aggregation_requests_reserved_[std::move(agg_key)];
  destination_vector.push_back(
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(
                  blink::mojom::AggregatableReportHistogramContribution::New(
                      *bucket_base + report_buyers_config->bucket,
                      base::saturated_cast<int32_t>(
                          std::max(0.0, value * report_buyers_config->scale)),
                      /*filtering_id=*/std::nullopt)),
          // TODO(caraitto): Consider allowing this to be set.
          blink::mojom::AggregationServiceMode::kDefault,
          std::move(debug_mode_details)));
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
    case AuctionResult::kInvalidServerResponse:
    case AuctionResult::kInvalidAuctionNonce:
      return false;
  }
}

bool InterestGroupAuction::NonKAnonWinnerIsKAnon() const {
  return top_non_kanon_enforced_bid() &&
         top_non_kanon_enforced_bid()
                 ->bid->auction->top_non_kanon_enforced_bid()
                 ->bid->bid_role ==
             auction_worklet::mojom::BidRole::kBothKAnonModes;
}

bool InterestGroupAuction::HasInterestGroups() const {
  if (!buyer_helpers_.empty()) {
    return true;
  }
  if (is_server_auction_) {
    return true;
  }
  for (const auto& kv : component_auctions_) {
    if (!kv.second->buyer_helpers_.empty()) {
      return true;
    }
  }
  return false;
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
  std::optional<PostAuctionSignals> top_level_signals;
  std::optional<url::Origin> top_level_seller;
  if (parent_) {
    top_level_signals.emplace();
    top_level_seller = parent_->config_->seller;
  }

  if (!leader.top_bid) {
    DCHECK_EQ(leader.highest_scoring_other_bid, 0);
    DCHECK(!leader.highest_scoring_other_bid_owner.has_value());
  }

  // Summarize various metrics we collected in format convenient for using them
  // as private aggregation base values. If `parent_ != nullptr`, it would have
  // called this before it recursed to us.
  FillInSellerParticipantDataMetrics();

  // Figure out appropriate seller reps for "reserved.once", and which metrics
  // go to which level.
  const BidState* non_top_level_seller_once_rep;
  const PrivateAggregationParticipantData* non_top_level_seller_data;
  const BidState* top_level_seller_once_rep;
  const PrivateAggregationParticipantData* top_level_seller_data;
  if (parent_) {
    non_top_level_seller_once_rep = seller_reserved_once_rep_;
    non_top_level_seller_data = &seller_metrics_;
    top_level_seller_once_rep = parent_->seller_reserved_once_rep_;
    top_level_seller_data = &parent_->seller_metrics_;
  } else {
    non_top_level_seller_once_rep = nullptr;
    non_top_level_seller_data = nullptr;
    top_level_seller_once_rep = seller_reserved_once_rep_;
    top_level_seller_data = &seller_metrics_;
  }

  std::map<PrivateAggregationKey, PrivateAggregationRequests>
      private_aggregation_requests_reserved;
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_requests_non_reserved;
  std::map<url::Origin, InterestGroupAuction::RealTimeReportingContributions>
      real_time_contributions;

  for (const auto& buyer_helper : buyer_helpers_) {
    ComputePostAuctionSignals(buyer_helper->owner(), signals,
                              top_level_signals);
    buyer_helper->TakeDebugReportUrls(
        winner, signals, top_level_signals, config_->seller, top_level_seller,
        debug_win_report_urls, debug_loss_report_urls);

    buyer_helper->TakePrivateAggregationRequests(
        winner, non_kanon_winner, signals, top_level_signals,
        non_top_level_seller_once_rep, top_level_seller_once_rep,
        non_top_level_seller_data, top_level_seller_data,
        private_aggregation_requests_reserved,
        private_aggregation_requests_non_reserved);

    buyer_helper->TakeRealTimeContributions(real_time_contributions);
  }

  PrivateAggregationReservedOnceReps additional_bid_reps;
  PrivateAggregationAllParticipantsDataPtrs additional_bid_data;
  additional_bid_reps[static_cast<size_t>(
      PrivateAggregationPhase::kTopLevelSeller)] = top_level_seller_once_rep;
  additional_bid_data[static_cast<size_t>(
      PrivateAggregationPhase::kTopLevelSeller)] = top_level_seller_data;
  additional_bid_reps[static_cast<size_t>(
      PrivateAggregationPhase::kNonTopLevelSeller)] =
      non_top_level_seller_once_rep;
  additional_bid_data[static_cast<size_t>(
      PrivateAggregationPhase::kNonTopLevelSeller)] = non_top_level_seller_data;
  additional_bid_reps[static_cast<size_t>(PrivateAggregationPhase::kBidder)] =
      nullptr;
  additional_bid_data[static_cast<size_t>(PrivateAggregationPhase::kBidder)] =
      nullptr;

  for (std::unique_ptr<BidState>& bid_state : bid_states_for_additional_bids_) {
    const url::Origin& owner = bid_state->additional_bid_buyer.value();
    ComputePostAuctionSignals(owner, signals, top_level_signals);
    TakePrivateAggregationRequestsForBidState(
        bid_state, /*is_component_auction=*/parent_ != nullptr, winner,
        non_kanon_winner, additional_bid_reps, signals, top_level_signals,
        additional_bid_data, private_aggregation_requests_reserved,
        private_aggregation_requests_non_reserved);
    TakeDebugReportUrlsForBidState(
        bid_state, winner, signals, top_level_signals, owner, config_->seller,
        top_level_seller, debug_report_lockout_and_cooldowns_,
        new_debug_report_lockout_and_cooldowns_, debug_win_report_urls,
        debug_loss_report_urls);
    TakeRealTimeContributionsForBidState(*bid_state, real_time_contributions);
  }

  for (auto& [key, requests] : private_aggregation_requests_reserved) {
    PrivateAggregationRequests& destination_vector =
        private_aggregation_requests_reserved_[key];
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

  for (auto& [origin, contributions] : real_time_contributions) {
    InterestGroupAuction::RealTimeReportingContributions& destination_vector =
        real_time_contributions_[origin];
    destination_vector.insert(destination_vector.end(),
                              std::move_iterator(contributions.begin()),
                              std::move_iterator(contributions.end()));
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction_info : component_auctions_) {
    component_auction_info.second
        ->TakeDebugReportUrlsAndFillInPrivateAggregationRequests(
            debug_win_report_urls, debug_loss_report_urls);
  }

  if (new_debug_report_lockout_and_cooldowns_.last_report_sent_time
          .has_value()) {
    interest_group_manager_->RecordDebugReportLockout(
        *new_debug_report_lockout_and_cooldowns_.last_report_sent_time);
  }
  for (const auto& [origin, debug_report_cooldown] :
       new_debug_report_lockout_and_cooldowns_.debug_report_cooldown_map) {
    interest_group_manager_->RecordDebugReportCooldown(
        origin, debug_report_cooldown.starting_time,
        debug_report_cooldown.type);
  }
}

std::map<PrivateAggregationKey,
         InterestGroupAuction::PrivateAggregationRequests>
InterestGroupAuction::TakeReservedPrivateAggregationRequests() {
  for (auto& component_auction_info : component_auctions_) {
    std::map<PrivateAggregationKey, PrivateAggregationRequests> requests_map =
        component_auction_info.second->TakeReservedPrivateAggregationRequests();
    for (auto& [agg_key, requests] : requests_map) {
      CHECK(!requests.empty());
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_reserved_[agg_key];
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
      CHECK(!requests.empty());
      PrivateAggregationRequests& destination_vector =
          private_aggregation_requests_non_reserved_[event_type];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(requests.begin()),
                                std::move_iterator(requests.end()));
    }
  }
  return std::move(private_aggregation_requests_non_reserved_);
}

InterestGroupAuctionReporter::PrivateAggregationAllParticipantsData
InterestGroupAuction::ComputePrivateAggregationParticipantData() {
  InterestGroupAuctionReporter::PrivateAggregationAllParticipantsData
      all_participant_data;

  ScoredBid* winner = leader_info().top_bid.get();
  BidState* winner_bid_state = winner->bid->bid_state;

  // For non-additional bids, find their BuyerHelper to get their metrics.
  if (!winner_bid_state->additional_bid_buyer) {
    const url::Origin& bid_origin =
        winner_bid_state->bidder->interest_group.owner;
    for (const auto& buyer_helper : winner->bid->auction->buyer_helpers_) {
      if (buyer_helper->owner() == bid_origin) {
        all_participant_data[static_cast<size_t>(
            PrivateAggregationPhase::kBidder)] = buyer_helper->buyer_metrics();
        break;
      }
    }
  }

  // `this` is always a top-level seller.
  all_participant_data[static_cast<size_t>(
      PrivateAggregationPhase::kTopLevelSeller)] = seller_metrics_;

  if (winner->bid->auction != this) {
    // There is a component seller as well.
    all_participant_data[static_cast<size_t>(
        PrivateAggregationPhase::kNonTopLevelSeller)] =
        winner->bid->auction->seller_metrics_;
  }
  return all_participant_data;
}

std::map<url::Origin, InterestGroupAuction::RealTimeReportingContributions>
InterestGroupAuction::TakeRealTimeReportingContributions() {
  for (auto& component_auction_info : component_auctions_) {
    std::map<url::Origin, RealTimeReportingContributions> contributions_map =
        component_auction_info.second->TakeRealTimeReportingContributions();
    for (auto& [origin, contributions] : contributions_map) {
      RealTimeReportingContributions& destination_vector =
          real_time_contributions_[origin];
      destination_vector.insert(destination_vector.end(),
                                std::move_iterator(contributions.begin()),
                                std::move_iterator(contributions.end()));
    }
  }
  return std::move(real_time_contributions_);
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

base::flat_set<url::Origin> InterestGroupAuction::GetSellersAndBuyers() {
  std::vector<url::Origin> origins;
  origins.push_back(config_->seller);
  for (const auto& buyer_helper : buyer_helpers_) {
    origins.push_back(buyer_helper->owner());
  }

  for (const auto& component_auction_info : component_auctions_) {
    InterestGroupAuction* component_auction =
        component_auction_info.second.get();
    origins.push_back(component_auction->config_->seller);
    for (const auto& buyer_helper : component_auction->buyer_helpers_) {
      origins.push_back(buyer_helper->owner());
    }
  }
  return base::flat_set<url::Origin>(std::move(origins));
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
    if (scored_bid->bid->bid_state->additional_bid_buyer.has_value()) {
      continue;
    }
    DCHECK(scored_bid->bid);
    const blink::InterestGroup& interest_group =
        *scored_bid->bid->interest_group;
    k_anon_keys_to_join.push_back(blink::HashedKAnonKeyForAdBid(
        interest_group, scored_bid->bid->bid_ad->render_url()));
    k_anon_keys_to_join.push_back(blink::HashedKAnonKeyForAdNameReporting(
        interest_group, *scored_bid->bid->bid_ad,
        scored_bid->bid->selected_buyer_and_seller_reporting_id));
    for (const blink::AdDescriptor& ad_component_descriptor :
         scored_bid->bid->ad_component_descriptors) {
      k_anon_keys_to_join.push_back(
          blink::HashedKAnonKeyForAdComponentBid(ad_component_descriptor));
    }
  }
  return base::flat_set<std::string>(std::move(k_anon_keys_to_join));
}

void InterestGroupAuction::MaybeLogPrivateAggregationWebFeatures(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        private_aggregation_requests) {
  CHECK(maybe_log_private_aggregation_web_features_callback_);
  maybe_log_private_aggregation_web_features_callback_.Run(
      private_aggregation_requests);
}

bool InterestGroupAuction::
    BlockDueToDisallowedCrossOriginTrustedSellerSignals() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    return false;
  }

  if (!config_->trusted_scoring_signals_url.has_value()) {
    return false;
  }

  url::Origin trusted_scoring_signals_origin =
      url::Origin::Create(config_->trusted_scoring_signals_url.value());

  if (config_->seller.IsSameOriginWith(trusted_scoring_signals_origin)) {
    return false;
  }

  if (is_interest_group_api_allowed_callback_.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          trusted_scoring_signals_origin)) {
    return false;
  }

  errors_.push_back(base::StringPrintf(
      "runAdAuction() auction with seller '%s' failed because it lacks "
      "attestation of cross-origin trusted signals origin '%s' or that origin "
      "is disallowed by user preferences",
      config_->seller.Serialize().c_str(),
      trusted_scoring_signals_origin.Serialize().c_str()));

  return true;
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

void InterestGroupAuction::ComputePostAuctionSignals(
    const url::Origin& bid_owner,
    PostAuctionSignals& signals_out,
    std::optional<PostAuctionSignals>& top_level_signals_out) {
  DCHECK(!parent_ || top_level_signals_out.has_value());
  const LeaderInfo& leader = leader_info();
  if (leader.top_bid) {
    PostAuctionSignals::FillWinningBidInfo(
        bid_owner, leader.top_bid->bid->interest_group->owner,
        leader.top_bid->bid->bid, leader.top_bid->bid_in_seller_currency,
        config_->non_shared_params.seller_currency,
        signals_out.made_winning_bid, signals_out.winning_bid,
        signals_out.winning_bid_currency);
  }

  PostAuctionSignals::FillRelevantHighestScoringOtherBidInfo(
      bid_owner, leader.highest_scoring_other_bid_owner,
      leader.highest_scoring_other_bid,
      leader.highest_scoring_other_bid_in_seller_currency,
      config_->non_shared_params.seller_currency,
      signals_out.made_highest_scoring_other_bid,
      signals_out.highest_scoring_other_bid,
      signals_out.highest_scoring_other_bid_currency);

  if (parent_ && parent_->top_bid()) {
    PostAuctionSignals::FillWinningBidInfo(
        bid_owner, parent_->top_bid()->bid->interest_group->owner,
        parent_->top_bid()->bid->bid,
        parent_->top_bid()->bid_in_seller_currency,
        parent_->config_->non_shared_params.seller_currency,
        top_level_signals_out->made_winning_bid,
        top_level_signals_out->winning_bid,
        top_level_signals_out->winning_bid_currency);
  }
}

void InterestGroupAuction::FillInSellerParticipantDataMetrics() {
  if (code_fetch_time_.GetNumRecords() != 0) {
    seller_metrics_.average_code_fetch_time = code_fetch_time_.GetMeanLatency();
  }
  seller_metrics_.percent_scripts_timeout =
      PercentMetric(seller_scripts_timed_out_, seller_scripts_ran_);
}

uint16_t InterestGroupAuction::GetBuyerMultiBidLimit(const url::Origin& buyer) {
  uint16_t val = config_->non_shared_params.all_buyers_multi_bid_limit;
  auto it = config_->non_shared_params.per_buyer_multi_bid_limits.find(buyer);
  if (it != config_->non_shared_params.per_buyer_multi_bid_limits.end()) {
    val = it->second;
  }
  return std::max(val, uint16_t{1});
}

std::optional<uint16_t> InterestGroupAuction::GetBuyerExperimentId(
    const blink::AuctionConfig& config,
    const url::Origin& buyer) {
  auto it = config.per_buyer_experiment_group_ids.find(buyer);
  if (it != config.per_buyer_experiment_group_ids.end()) {
    return it->second;
  }
  return config.all_buyer_experiment_group_id;
}

std::string InterestGroupAuction::CreateTrustedBiddingSignalsSlotSizeParam(
    const blink::AuctionConfig& config,
    blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode
        trusted_bidding_signals_slot_size_mode) {
  switch (trusted_bidding_signals_slot_size_mode) {
    case blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone:
      return std::string();

    case blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::kSlotSize:
      if (!config.non_shared_params.requested_size) {
        return std::string();
      }
      return "slotSize=" + blink::ConvertAdSizeToString(
                               *config.non_shared_params.requested_size);

    case blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
        kAllSlotsRequestedSizes: {
      if (!config.non_shared_params.all_slots_requested_sizes ||
          config.non_shared_params.all_slots_requested_sizes->empty()) {
        return std::string();
      }

      std::string all_ad_sizes;
      for (const blink::AdSize& ad_size :
           *config.non_shared_params.all_slots_requested_sizes) {
        if (!all_ad_sizes.empty()) {
          all_ad_sizes += ",";
        }
        all_ad_sizes += blink::ConvertAdSizeToString(ad_size);
      }
      return std::string("allSlotsRequestedSizes=" + all_ad_sizes);
    }
  }
}

std::optional<std::string> InterestGroupAuction::GetPerBuyerSignals(
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
  return std::nullopt;
}

std::optional<GURL> InterestGroupAuction::GetDirectFromSellerAuctionSignals(
    const SubresourceUrlBuilder* subresource_url_builder) {
  if (subresource_url_builder && subresource_url_builder->auction_signals()) {
    return subresource_url_builder->auction_signals()->subresource_url;
  }
  return std::nullopt;
}

std::optional<std::string>
InterestGroupAuction::GetDirectFromSellerAuctionSignalsHeaderAdSlot(
    const HeaderDirectFromSellerSignals::Result& signals) {
  return signals.auction_signals();
}

std::optional<GURL> InterestGroupAuction::GetDirectFromSellerPerBuyerSignals(
    const SubresourceUrlBuilder* subresource_url_builder,
    const url::Origin& owner) {
  if (!subresource_url_builder) {
    return std::nullopt;
  }

  auto it = subresource_url_builder->per_buyer_signals().find(owner);
  if (it == subresource_url_builder->per_buyer_signals().end()) {
    return std::nullopt;
  }
  return it->second.subresource_url;
}

std::optional<std::string>
InterestGroupAuction::GetDirectFromSellerPerBuyerSignalsHeaderAdSlot(
    const HeaderDirectFromSellerSignals::Result& signals,
    const url::Origin& owner) {
  auto it = signals.per_buyer_signals().find(owner);
  if (it == signals.per_buyer_signals().end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<GURL> InterestGroupAuction::GetDirectFromSellerSellerSignals(
    const SubresourceUrlBuilder* subresource_url_builder) {
  if (subresource_url_builder && subresource_url_builder->seller_signals()) {
    return subresource_url_builder->seller_signals()->subresource_url;
  }
  return std::nullopt;
}

std::optional<std::string>
InterestGroupAuction::GetDirectFromSellerSellerSignalsHeaderAdSlot(
    const HeaderDirectFromSellerSignals::Result& signals) {
  return signals.seller_signals();
}

const std::vector<blink::AuctionConfig::AdKeywordReplacement>&
InterestGroupAuction::GetDeprecatedRenderURLReplacements() {
  return config_->non_shared_params.deprecated_render_url_replacements.value();
}

InterestGroupAuction::LeaderInfo::LeaderInfo() = default;
InterestGroupAuction::LeaderInfo::~LeaderInfo() = default;

void InterestGroupAuction::OnInterestGroupRead(
    scoped_refptr<StorageInterestGroups> read_interest_groups) {
  ++num_owners_loaded_;
  if (read_interest_groups->size() == 0) {
    OnOneLoadCompleted();
    return;
  }
  std::vector<SingleStorageInterestGroup> interest_groups =
      read_interest_groups->GetInterestGroups();

  // Compute PA base values about resource usage.
  int positive_groups = 0;
  int negative_groups = 0;
  size_t storage_used = 0;
  for (const SingleStorageInterestGroup& group : interest_groups) {
    if (group->interest_group.additional_bid_key.has_value()) {
      ++negative_groups;
    } else {
      ++positive_groups;
    }

    storage_used += group->interest_group.EstimateSize();
  }

  // Report info on read as a delegatable metric as well.
  for (const SingleStorageInterestGroup& group : interest_groups) {
    if (ReportInterestGroupCount(group->interest_group,
                                 read_interest_groups->size())) {
      break;
    }
  }

  post_auction_update_owners_.emplace_back(
      interest_groups[0]->interest_group.owner);
  for (const SingleStorageInterestGroup& bidder : interest_groups) {
    // Report freshness metrics.
    if (bidder->interest_group.update_url.has_value()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Ads.InterestGroup.Auction.GroupFreshness.WithDailyUpdates",
          (base::Time::Now() - bidder->last_updated).InMinutes(),
          kGroupFreshnessMin.InMinutes(), kGroupFreshnessMax.InMinutes(),
          kGroupFreshnessBuckets);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Ads.InterestGroup.Auction.GroupFreshness.NoDailyUpdates",
          (base::Time::Now() - bidder->last_updated).InMinutes(),
          kGroupFreshnessMin.InMinutes(), kGroupFreshnessMax.InMinutes(),
          kGroupFreshnessBuckets);
    }
  }

  // Extract negative targeting info, if something may care.
  // This needs to happen before dropping IGs w/o scripts and ads, since
  // negative targeting IGs may lack those.
  if (MayHaveAdditionalBids()) {
    for (const SingleStorageInterestGroup& group : interest_groups) {
      if (group->interest_group.additional_bid_key.has_value()) {
        negative_targeter_->AddInterestGroupInfo(
            group->interest_group.owner, group->interest_group.name,
            group->joining_origin, *group->interest_group.additional_bid_key);
      }
    }
  }

  // Track interest group memory consumption for metrics. This is done before
  // dropping interest groups to get a ceiling estimate of max memory
  // consumption for ads and adComponents at auction.
  UpdateIgSizeMetrics(interest_groups);

  // Ignore interest groups with no bidding script or no ads.
  std::erase_if(interest_groups, [](const SingleStorageInterestGroup& bidder) {
    return !bidder->interest_group.bidding_url || !bidder->interest_group.ads ||
           bidder->interest_group.ads->empty();
  });

  // Ignore interest groups that don't provide the requested seller
  // capabilities.
  std::erase_if(interest_groups,
                [this](const SingleStorageInterestGroup& bidder) {
                  return !GroupSatisfiesAllCapabilities(
                      bidder->interest_group,
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
      interest_groups[0]->interest_group.owner);
  auto buyer_helper =
      std::make_unique<BuyerHelper>(this, std::move(interest_groups));
  buyer_helper->SetStorageMetrics(positive_groups, negative_groups,
                                  storage_used);
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

  // Erase component auctions that failed their interest group loading phase,
  // so they won't be invoked in the generate bid phase. This is not a problem
  // in the reporting phase, as the top-level auction knows which component
  // auction, if any, won.
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
      size_t num_sellers_with_bidders = 0;
      size_t total_interest_group_bytes_for_metrics = 0u;
      size_t total_ads_and_ad_components_bytes_for_metrics = 0u;
      for (const auto& [unused, component] : component_auctions_) {
        if (!component->buyer_helpers_.empty()) {
          ++num_sellers_with_bidders;
        }
        total_interest_group_bytes_for_metrics +=
            component->interest_groups_bytes_for_metrics_;
        total_ads_and_ad_components_bytes_for_metrics +=
            component->ads_and_ad_components_bytes_for_metrics_;
      }
      base::UmaHistogramMemoryKB("Ads.InterestGroup.AtAuctionTotalSize.Groups",
                                 total_interest_group_bytes_for_metrics / 1024);
      base::UmaHistogramMemoryKB(
          "Ads.InterestGroup.AtAuctionTotalSize.AdsAndAdComponents",
          total_ads_and_ad_components_bytes_for_metrics / 1024);

      // If the top-level seller either has interest groups itself, or any of
      // the component auctions do, then the top-level seller also has
      // bidders.
      if (num_interest_groups > 0) {
        ++num_sellers_with_bidders;
      }

      auction_metrics_recorder_->SetNumInterestGroups(num_interest_groups);
      auction_metrics_recorder_->SetNumOwnersWithInterestGroups(
          num_owners_with_interest_groups_);
      auction_metrics_recorder_->SetNumSellersWithBidders(
          num_sellers_with_bidders);

      // Count the owners that passed the `IsInterestGroupApiAllowedCallback`
      // filter, but were then excluded due to having no ads or no script URL.
      // Double counts such buyers that participate in multiple auctions.
      CHECK_GE(num_owners_loaded_, num_owners_with_interest_groups_);
      auction_metrics_recorder_->SetNumOwnersWithoutInterestGroups(
          num_owners_loaded_ - num_owners_with_interest_groups_);
    }
  }

  if (MayHaveAdditionalBids()) {
    auction_metrics_recorder_->RecordNegativeInterestGroups(
        negative_targeter_->GetNumNegativeInterestGroups());
  }

  // We generally proceed even if there is seemingly nothing to do since we
  // still may need to wait for promises to resolve and deal with
  // `additional_bids`.
  AuctionResult result = AuctionResult::kSuccess;

  // If the config had components, but none survived the loading phase,
  // they must all have failed permissions checks, so there is no reason
  // to proceed.
  if (!config_->non_shared_params.component_auctions.empty() &&
      component_auctions_.empty()) {
    result = AuctionResult::kNoInterestGroups;
  }

  OnStartLoadInterestGroupsPhaseComplete(result);
}

void InterestGroupAuction::OnStartLoadInterestGroupsPhaseComplete(
    AuctionResult auction_result) {
  DCHECK(load_interest_groups_phase_callback_);
  DCHECK(!final_auction_result_);

  if (!parent_) {
    auction_metrics_recorder_->OnLoadInterestGroupPhaseComplete();
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "load_groups_phase", *trace_id_);

  if (!HasInterestGroups()) {
    UMA_HISTOGRAM_TIMES("Ads.InterestGroup.Auction.LoadNoGroupsTime",
                        base::TimeTicks::Now() - creation_time_);
  } else {
    UMA_HISTOGRAM_TIMES("Ads.InterestGroup.Auction.LoadGroupsTime",
                        base::TimeTicks::Now() - creation_time_);
  }

  // `final_auction_result_` should only be set to kSuccess when the entire
  // auction is complete.
  //
  // TODO(crbug.com/40248758): We should probably add new states for
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
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "request_seller_worklet",
                                    *trace_id_);
  auction_worklet_manager_->RequestSellerWorklet(
      devtools_auction_id_, *config_->decision_logic_url,
      config_->trusted_scoring_signals_url, config_->seller_experiment_group_id,
      config_->non_shared_params.trusted_scoring_signals_coordinator,
      base::BindOnce(&InterestGroupAuction::OnSellerWorkletReceived,
                     base::Unretained(this)),
      base::BindOnce(&InterestGroupAuction::OnSellerWorkletFatalError,
                     base::Unretained(this)),
      seller_worklet_handle_, auction_metrics_recorder_);
}

void InterestGroupAuction::OnSellerWorkletReceived() {
  DCHECK(!seller_worklet_received_);
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);

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

    if (!component_auctions_.empty()) {
      auction_metrics_recorder_->RecordTopLevelBidQueuedWaitingForSellerWorklet(
          unscored_bid->wait_worklet);
    } else {
      auction_metrics_recorder_->RecordBidQueuedWaitingForSellerWorklet(
          unscored_bid->wait_worklet);
    }
  }

  ScoreQueuedBidsIfReady();
}

void InterestGroupAuction::ScoreQueuedBidsIfReady() {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (!ReadyToScoreBids() || unscored_bids_.empty()) {
    return;
  }

  auto unscored_bids = std::move(unscored_bids_);
  for (auto& unscored_bid : unscored_bids) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "fledge", "wait_for_seller_deps", unscored_bid->TraceIdForScoring(),
        "data", [&](perfetto::TracedValue trace_context) {
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
    ScoreBid(std::move(unscored_bid));
  }

  // If no further bids are outstanding, now is the time to send a coalesced
  // request for all the trusted seller signals (if some still are pending,
  // OnScoringDependencyDone() will take care of it).
  if (num_scoring_dependencies_ == 0) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
  }

  // No more unscored bids should be added, once the seller worklet has been
  // received.
  DCHECK(unscored_bids_.empty());
}

void InterestGroupAuction::HandleUpdateIfOlderThan(
    const url::Origin& owner,
    std::string_view name,
    std::optional<base::TimeDelta> update_if_older_than) {
  if (!base::FeatureList::IsEnabled(
          features::kInterestGroupUpdateIfOlderThan)) {
    return;
  }
  if (!update_if_older_than) {
    return;
  }
  base::UmaHistogramCustomTimes(
      "Ads.InterestGroup.UpdateIfOlderThan", *update_if_older_than,
      kMinUpdateIfOlderThanHistogram, kMaxUpdateIfOlderThanHistogram,
      kUpdateIfOlderThanBuckets);
  if (*update_if_older_than < base::Minutes(10)) {
    *update_if_older_than = base::Minutes(10);
  }
  interest_group_manager_->AllowUpdateIfOlderThan(
      blink::InterestGroupKey(owner, std::string(name)), *update_if_older_than);
}

void InterestGroupAuction::HandleAdditionalBidError(AdditionalBidResult result,
                                                    std::string error) {
  auction_metrics_recorder_->RecordAdditionalBidResult(result);
  errors_.push_back(std::move(error));
  OnScoringDependencyDone();
}

void InterestGroupAuction::DecodeAdditionalBidsIfReady() {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (encoded_signed_additional_bids_.empty()) {
    return;
  }
  decode_additional_bids_start_time_ = base::TimeTicks::Now();

  data_decoder::DataDecoder* data_decoder =
      get_data_decoder_callback_.Run(config_->seller);

  num_scoring_dependencies_ += encoded_signed_additional_bids_.size();
  for (const auto& encoded_signed_bid : encoded_signed_additional_bids_) {
    std::string signed_additional_bid_data;
    if (!base::Base64Decode(encoded_signed_bid, &signed_additional_bid_data,
                            base::Base64DecodePolicy::kForgiving)) {
      HandleAdditionalBidError(
          AdditionalBidResult::kRejectedDueToInvalidBase64,
          "Unable to base64-decode a signed additional bid.");
      continue;
    }
    if (!data_decoder) {
      HandleAdditionalBidError(AdditionalBidResult::kRejectedDecoderShutDown,
                               "Page in shutdown.");
      continue;
    }
    data_decoder->ParseJson(
        signed_additional_bid_data,
        base::BindOnce(&InterestGroupAuction::HandleDecodedSignedAdditionalBid,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  encoded_signed_additional_bids_.clear();
  currently_decoding_additional_bids_ = true;
}

void InterestGroupAuction::HandleDecodedSignedAdditionalBid(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (!result.has_value()) {
    HandleAdditionalBidError(
        AdditionalBidResult::kRejectedDueToSignedBidJsonParseError,
        "Unable to parse signed additional bid as JSON: " + result.error());
    return;
  }

  auto maybe_signed_additional_bid =
      DecodeSignedAdditionalBid(std::move(result).value());
  if (!maybe_signed_additional_bid.has_value()) {
    HandleAdditionalBidError(
        AdditionalBidResult::kRejectedDueToSignedBidDecodeError,
        "Unable to decode signed additional bid: " +
            maybe_signed_additional_bid.error());
    return;
  }

  auto valid_signatures = maybe_signed_additional_bid->VerifySignatures();

  data_decoder::DataDecoder* data_decoder =
      get_data_decoder_callback_.Run(config_->seller);
  if (!data_decoder) {
    HandleAdditionalBidError(AdditionalBidResult::kRejectedDecoderShutDown,
                             "Page in shutdown");
    return;
  }

  data_decoder->ParseJson(
      maybe_signed_additional_bid->additional_bid_json,
      base::BindOnce(&InterestGroupAuction::HandleDecodedAdditionalBid,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(maybe_signed_additional_bid->signatures),
                     std::move(valid_signatures)));
}

void InterestGroupAuction::HandleDecodedAdditionalBid(
    const std::vector<SignedAdditionalBidSignature>& signatures,
    const std::vector<size_t>& valid_signatures,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (!result.has_value()) {
    HandleAdditionalBidError(
        AdditionalBidResult::kRejectedDueToJsonParseError,
        "Unable to parse additional bid as JSON: " + result.error());
    return;
  }

  base::expected<AdditionalBidDecodeResult, std::string> maybe_bid =
      DecodeAdditionalBid(
          this, result.value(),
          config_->non_shared_params.auction_nonce.value(),
          interest_group_buyers_, config_->seller,
          parent_
              ? base::optional_ref<const url::Origin>(parent_->config_->seller)
              : base::optional_ref<const url::Origin>(std::nullopt));
  if (!maybe_bid.has_value()) {
    HandleAdditionalBidError(AdditionalBidResult::kRejectedDueToDecodeError,
                             std::move(maybe_bid).error());
    return;
  }

  if (!is_interest_group_api_allowed_callback_.Run(
          ContentBrowserClient::InterestGroupApiOperation::kBuy,
          maybe_bid->bid_state->bidder->interest_group.owner)) {
    HandleAdditionalBidError(
        AdditionalBidResult::kRejectedDueToBuyerNotAllowed,
        "Rejecting an additionalBid due to operation not allowed for buyer.");
    return;
  }

  if (!blink::VerifyAdCurrencyCode(
          PerBuyerCurrency(*maybe_bid->bid_state->additional_bid_buyer,
                           *config_),
          maybe_bid->bid->bid_currency)) {
    HandleAdditionalBidError(
        AdditionalBidResult::kRejectedDueToCurrencyMismatch,
        "Rejecting an additionalBid due to currency mismatch.");
    return;
  }

  if (negative_targeter_->ShouldDropDueToNegativeTargeting(
          *maybe_bid->bid_state->additional_bid_buyer,
          maybe_bid->negative_target_joining_origin,
          maybe_bid->negative_target_interest_group_names, signatures,
          valid_signatures, config_->seller, *auction_metrics_recorder_,
          errors_)) {
    // We do *not* call HandleAdditionalBidError at this point because an
    // additional bid being negative targeted is not an error scenario, so we
    // don't want to record anything to the errors_.
    auction_metrics_recorder_->RecordAdditionalBidResult(
        AdditionalBidResult::kNegativeTargeted);
    auction_metrics_recorder_->RecordAdditionalBidDecodeLatency(
        base::TimeTicks::Now() - decode_additional_bids_start_time_);
    OnScoringDependencyDone();
    return;
  }

  auction_metrics_recorder_->RecordAdditionalBidResult(
      AdditionalBidResult::kSentForScoring);
  auction_metrics_recorder_->RecordAdditionalBidDecodeLatency(
      base::TimeTicks::Now() - decode_additional_bids_start_time_);
  bid_states_for_additional_bids_.push_back(std::move(maybe_bid->bid_state));
  ScoreBidIfReady(std::move(maybe_bid->bid));
  OnScoringDependencyDone();
}

void InterestGroupAuction::OnSellerWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  AuctionResult result;
  switch (fatal_error_type) {
    case AuctionWorkletManager::FatalErrorType::kScriptLoadFailed:
      result = AuctionResult::kSellerWorkletLoadFailed;
      MaybeAddScriptFailureRealTimeContribution(/*is_buyer=*/false,
                                                config_->seller);
      break;
    case AuctionWorkletManager::FatalErrorType::kWorkletCrash:
      result = AuctionResult::kSellerWorkletCrashed;
      break;
  }

  OnBiddingAndScoringComplete(result, errors);
}

bool InterestGroupAuction::IsBuyerOptedInToRealTimeReporting(
    const url::Origin& owner) {
  return config_->non_shared_params.per_buyer_real_time_reporting_types
             .has_value() &&
         base::Contains(
             *config_->non_shared_params.per_buyer_real_time_reporting_types,
             owner);
}

void InterestGroupAuction::MaybeAddScriptFailureRealTimeContribution(
    bool is_buyer,
    const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeRealTimeReporting)) {
    return;
  }
  if (!real_time_reporting_num_buckets_.has_value()) {
    real_time_reporting_num_buckets_ =
        blink::features::kFledgeRealTimeReportingNumBuckets.Get();
  }
  if (!real_time_platform_contribution_priority_weight_.has_value()) {
    real_time_platform_contribution_priority_weight_ =
        blink::features::kFledgeRealTimeReportingPlatformContributionPriority
            .Get();
  }
  if (is_buyer && IsBuyerOptedInToRealTimeReporting(origin)) {
    real_time_contributions_[origin].push_back(
        auction_worklet::mojom::RealTimeReportingContribution::New(
            *real_time_reporting_num_buckets_ +
                auction_worklet::RealTimeReportingPlatformError::
                    kBiddingScriptLoadFailure,
            *real_time_platform_contribution_priority_weight_,
            /*latency_threshold=*/std::nullopt));
  } else if (!is_buyer && config_->non_shared_params
                              .seller_real_time_reporting_type.has_value()) {
    real_time_contributions_[origin].push_back(
        auction_worklet::mojom::RealTimeReportingContribution::New(
            *real_time_reporting_num_buckets_ +
                auction_worklet::RealTimeReportingPlatformError::
                    kScoringScriptLoadFailure,
            *real_time_platform_contribution_priority_weight_,
            /*latency_threshold=*/std::nullopt));
  }
}

void InterestGroupAuction::OnComponentAuctionComplete(
    InterestGroupAuction* component_auction,
    bool success) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (!component_auction->buyer_helpers_.empty() ||
      !component_auction->bid_states_for_additional_bids_.empty()) {
    auction_metrics_recorder_->RecordComponentAuctionLatency(
        base::TimeTicks::Now() - bidding_and_scoring_phase_start_time_);
  }

  bool is_both = component_auction->NonKAnonWinnerIsKAnon();
  ScoredBid* non_kanon_enforced_bid =
      component_auction->top_non_kanon_enforced_bid();
  if (non_kanon_enforced_bid) {
    // There is no need to potentially turn this into an k-anon enforced bid
    // since that already happened when running the component auction.
    ScoreBidIfReady(CreateBidFromComponentAuctionWinner(
        non_kanon_enforced_bid,
        is_both ? auction_worklet::mojom::BidRole::kBothKAnonModes
                : auction_worklet::mojom::BidRole::kUnenforcedKAnon));
  }

  ScoredBid* kanon_bid = component_auction->top_kanon_enforced_bid();
  if (kanon_bid && !is_both) {
    ScoreBidIfReady(CreateBidFromComponentAuctionWinner(
        kanon_bid, auction_worklet::mojom::BidRole::kEnforcedKAnon));
  }

  OnScoringDependencyDone();
}

// static
std::unique_ptr<InterestGroupAuction::Bid>
InterestGroupAuction::CreateBidFromComponentAuctionWinner(
    const ScoredBid* scored_bid,
    auction_worklet::mojom::BidRole bid_role) {
  // Create a copy of component Auction's bid, replacing values as necessary.
  const Bid* component_bid = scored_bid->bid.get();
  const auto* modified_bid_params =
      scored_bid->component_auction_modified_bid_params.get();
  DCHECK(modified_bid_params);

  return std::make_unique<Bid>(
      bid_role, modified_bid_params->ad,
      modified_bid_params->bid.has_value() ? modified_bid_params->bid.value()
                                           : component_bid->bid,
      modified_bid_params->bid.has_value() ? modified_bid_params->bid_currency
                                           : component_bid->bid_currency,
      component_bid->ad_cost, component_bid->ad_descriptor,
      component_bid->ad_component_descriptors, component_bid->modeling_signals,
      component_bid->bid_duration, component_bid->bidding_signals_data_version,
      component_bid->bid_ad,
      component_bid->selected_buyer_and_seller_reporting_id,
      component_bid->bid_state, component_bid->auction);
}

void InterestGroupAuction::OnScoringDependencyDone() {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  --num_scoring_dependencies_;

  // If we issued the final set of bids to a seller worklet, tell it to send
  // any pending scoring signals request to complete the auction more quickly.
  if (num_scoring_dependencies_ == 0 && ReadyToScoreBids()) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
  }

  // If all scoring dependencies are done, this means additional bid decoding
  // in particular is.
  if (num_scoring_dependencies_ == 0) {
    currently_decoding_additional_bids_ = false;
  }

  MaybeCompleteBiddingAndScoringPhase();
}

void InterestGroupAuction::ScoreBidIfReady(std::unique_ptr<Bid> bid) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  DCHECK(bid);
  DCHECK(bid->bid_state->made_bid);

  any_bid_made_ = true;
  bid->BeginTracingForScoring();

  // TODO(crbug.com/41489627): Report k-anon re-runs.
  if (IsBidRoleUsedForWinner(kanon_mode_, bid->bid_role)) {
    InterestGroupManagerImpl::InterestGroupObserver::AccessType event_type =
        InterestGroupManagerImpl::InterestGroupObserver::kBid;
    if (!component_auctions_.empty()) {
      event_type =
          bid->bid_state->additional_bid_buyer
              ? InterestGroupManagerImpl::InterestGroupObserver::
                    kTopLevelAdditionalBid
              : InterestGroupManagerImpl::InterestGroupObserver::kTopLevelBid;
    } else if (bid->bid_state->additional_bid_buyer) {
      event_type =
          InterestGroupManagerImpl::InterestGroupObserver::kAdditionalBid;
    }

    interest_group_manager_->NotifyInterestGroupAccessed(
        devtools_auction_id_, event_type,
        bid->bid_state->bidder->interest_group.owner,
        bid->bid_state->bidder->interest_group.name,
        component_auctions_.empty() ? base::optional_ref<const url::Origin>()
                                    : base::optional_ref<const url::Origin>(
                                          bid->auction->config_->seller),
        bid->bid,
        bid->bid_currency
            ? base::optional_ref(bid->bid_currency->currency_code())
            : base::optional_ref<const std::string>());
  }

  // If seller worklet hasn't been received yet, or configuration is still
  // waiting on some promises, wait till everything is ready.
  // TODO(morlovich): Tracing doesn't reflect config wait here.
  uint64_t bid_trace_id = bid->TraceIdForScoring();
  if (!ReadyToScoreBids()) {
    bid->trace_wait_seller_deps_start = base::TimeTicks::Now();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "wait_for_seller_deps",
                                      bid_trace_id);
    unscored_bids_.emplace_back(std::move(bid));
    return;
  }
  ScoreBid(std::move(bid));
}

void InterestGroupAuction::ScoreBid(std::unique_ptr<Bid> bid) {
  DCHECK(ReadyToScoreBids());

  uint64_t bid_trace_id = bid->TraceIdForScoring();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("fledge", ScoreAdTraceEventName(*bid),
                                    bid_trace_id, "decision_logic_url",
                                    config_->decision_logic_url);
  bid->seller_worklet_score_ad_start = base::TimeTicks::Now();

  ++bids_being_scored_;
  Bid* bid_raw = bid.get();

  // We only pass the buyerAndSellerReportingId if there is a
  // selectedBuyerAndSellerReportingId.
  std::optional<std::string> maybe_buyer_and_seller_reporting_id;
  if (bid_raw->selected_buyer_and_seller_reporting_id.has_value()) {
    maybe_buyer_and_seller_reporting_id =
        bid_raw->bid_ad->buyer_and_seller_reporting_id;
  }

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
      GetDirectFromSellerSellerSignalsHeaderAdSlot(
          *direct_from_seller_signals_header_ad_slot_),
      GetDirectFromSellerAuctionSignals(url_builder),
      GetDirectFromSellerAuctionSignalsHeaderAdSlot(
          *direct_from_seller_signals_header_ad_slot_),
      GetOtherSellerParam(*bid_raw),
      parent_ ? PerBuyerCurrency(config_->seller, *parent_->config_)
              : std::nullopt,
      bid_raw->interest_group->owner, bid_raw->ad_descriptor.url,
      bid_raw->selected_buyer_and_seller_reporting_id,
      maybe_buyer_and_seller_reporting_id, bid_raw->GetAdComponentUrls(),
      bid_raw->bid_duration.InMilliseconds(), bid_raw->ad_descriptor.size,
      IsOriginInDebugReportCooldownOrLockout(
          config_->seller, debug_report_lockout_and_cooldowns_,
          base::Time::Now()),
      SellerTimeout(), bid_trace_id, bid_raw->bid_state->bidder->joining_origin,
      std::move(score_ad_remote));
}

bool InterestGroupAuction::ValidateScoreBidCompleteResult(
    double score,
    auction_worklet::mojom::ComponentAuctionModifiedBidParams*
        component_auction_modified_bid_params,
    std::optional<double> bid_in_seller_currency,
    const std::optional<GURL>& debug_loss_report_url,
    const std::optional<GURL>& debug_win_report_url,
    const PrivateAggregationRequests& pa_requests,
    const RealTimeReportingContributions& real_time_contributions) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
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

  if (!std::isfinite(score)) {
    score_ad_receivers_.ReportBadMessage("Invalid score");
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
        component_auction_modified_bid_params->bid.has_value()) {
      if (!IsValidBid(component_auction_modified_bid_params->bid.value())) {
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

  if (!ValidatePrivateAggregationRequests(score_ad_receivers_, pa_requests)) {
    return false;
  }

  if (!base::ranges::all_of(real_time_contributions, HasValidRealTimeBucket)) {
    score_ad_receivers_.ReportBadMessage("Invalid real time reporting bucket");
    return false;
  }

  if (!base::ranges::all_of(real_time_contributions,
                            HasValidRealTimePriorityWeight)) {
    score_ad_receivers_.ReportBadMessage(
        "Invalid real time reporting priority weight");
    return false;
  }

  return true;
}

void InterestGroupAuction::OnScoreAdComplete(
    double score,
    auction_worklet::mojom::RejectReason reject_reason,
    auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
        component_auction_modified_bid_params,
    std::optional<double> bid_in_seller_currency,
    std::optional<uint32_t> scoring_signals_data_version,
    const std::optional<GURL>& debug_loss_report_url,
    const std::optional<GURL>& debug_win_report_url,
    PrivateAggregationRequests pa_requests,
    RealTimeReportingContributions real_time_contributions,
    auction_worklet::mojom::SellerTimingMetricsPtr score_ad_timing_metrics,
    auction_worklet::mojom::ScoreAdDependencyLatenciesPtr
        score_ad_dependency_latencies,
    const std::vector<std::string>& errors) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  DCHECK_GT(bids_being_scored_, 0);

  if (!ValidateScoreBidCompleteResult(
          score, component_auction_modified_bid_params.get(),
          bid_in_seller_currency, debug_loss_report_url, debug_win_report_url,
          pa_requests, real_time_contributions)) {
    OnBiddingAndScoringComplete(AuctionResult::kBadMojoMessage);
    return;
  }

  std::unique_ptr<Bid> bid = std::move(score_ad_receivers_.current_context());
  score_ad_receivers_.Remove(score_ad_receivers_.current_receiver());

  auction_metrics_recorder_->RecordScoreAdFlowLatency(
      base::TimeTicks::Now() - bid->seller_worklet_score_ad_start);
  auction_metrics_recorder_->RecordScoreAdLatency(
      score_ad_timing_metrics->script_latency);
  auction_metrics_recorder_->RecordScoreAdDependencyLatencies(
      *score_ad_dependency_latencies);
  if (score_ad_timing_metrics->js_fetch_latency.has_value()) {
    code_fetch_time_.RecordLatency(*score_ad_timing_metrics->js_fetch_latency);
  }
  ++seller_scripts_ran_;
  if (score_ad_timing_metrics->script_timed_out) {
    ++seller_scripts_timed_out_;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", ScoreAdTraceEventName(*bid),
                                  bid->TraceIdForScoring());
  bid->EndTracingForScoring();
  bid->bid_state->pa_timings(seller_phase()).script_run_time =
      score_ad_timing_metrics->script_latency;
  bid->bid_state->pa_timings(seller_phase()).signals_fetch_time =
      score_ad_dependency_latencies->trusted_scoring_signals_latency.value_or(
          base::TimeDelta());

  --bids_being_scored_;

  // Reporting and the like should be done only for things that go into the
  // run that produces the result of runAdAuction().
  if (IsBidRoleUsedForWinner(kanon_mode_, bid->bid_role)) {
    // The mojom API declaration should ensure none of these are null.
    CHECK(base::ranges::none_of(
        pa_requests,
        [](const auction_worklet::mojom::PrivateAggregationRequestPtr&
               request_ptr) { return request_ptr.is_null(); }));
    MaybeLogPrivateAggregationWebFeatures(pa_requests);
    if (!pa_requests.empty()) {
      CHECK(config_);
      PrivateAggregationPhaseKey agg_key = {
          config_->seller, seller_phase(),
          config_->aggregation_coordinator_origin};
      PrivateAggregationRequests& pa_requests_for_seller =
          bid->bid_state->private_aggregation_requests[std::move(agg_key)];
      for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
           pa_requests) {
        // A for-event private aggregation request with non-reserved event
        // type from scoreAd() should be ignored and not reported.
        if (request->contribution->is_for_event_contribution() &&
            request->contribution->get_for_event_contribution()
                ->event_type->is_non_reserved()) {
          continue;
        }
        pa_requests_for_seller.emplace_back(std::move(request));
      }
    }

    // Update which of the executions gets used for 'reserved.once'.
    ++seller_reserved_once_rep_count_;
    if (seller_reserved_once_rep_count_ == 1 ||
        base::RandInt(1, seller_reserved_once_rep_count_) == 1) {
      seller_reserved_once_rep_ = bid->bid_state.get();
    }

    if (base::FeatureList::IsEnabled(
            blink::features::kFledgeRealTimeReporting) &&
        !base::FeatureList::IsEnabled(
            features::kCookieDeprecationFacilitatedTesting)) {
      // Only keep real time reporting contributions when the seller is
      // opted-in.
      if (config_->non_shared_params.seller_real_time_reporting_type
              .has_value()) {
        // Note that this adds an entry to the map for seller with an empty
        // vector as value, if seller was not in the map. This is important to
        // indicate that the seller is opted-in.
        RealTimeReportingContributions& real_time_contributions_for_origin =
            bid->bid_state->real_time_contributions[config_->seller];

        if (!real_time_contributions.empty()) {
          real_time_contributions_for_origin.insert(
              real_time_contributions_for_origin.end(),
              std::move_iterator(real_time_contributions.begin()),
              std::move_iterator(real_time_contributions.end()));
        }
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
      case auction_worklet::mojom::BidRole::kUnenforcedKAnon:
        UpdateAuctionLeaders(std::move(bid), score,
                             std::move(component_auction_modified_bid_params),
                             bid_in_seller_currency,
                             scoring_signals_data_version,
                             non_kanon_enforced_auction_leader_);
        break;
      case auction_worklet::mojom::BidRole::kEnforcedKAnon:
        UpdateAuctionLeaders(std::move(bid), score,
                             std::move(component_auction_modified_bid_params),
                             bid_in_seller_currency,
                             scoring_signals_data_version,
                             kanon_enforced_auction_leader_);
        break;
      case auction_worklet::mojom::BidRole::kBothKAnonModes: {
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
    std::optional<double> bid_in_seller_currency,
    std::optional<uint32_t> scoring_signals_data_version,
    LeaderInfo& leader_info) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
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
    // If the top bid is being replaced, need to add the old top bid as a
    // second highest bid. Otherwise, need to add the current bid as a second
    // highest bid.
    double new_highest_scoring_other_bid =
        is_top_bid ? leader_info.top_bid->bid->bid : bid->bid;
    std::optional<double> new_highest_scoring_other_bid_in_seller_currency =
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
    std::optional<double> bid_in_seller_currency,
    const url::Origin* owner,
    LeaderInfo& leader_info) {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);

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
  // In case of a tie, randomly pick one. This is the select random value from
  // a stream with fixed storage problem.
  if (1 == base::RandInt(1, leader_info.num_second_highest_bids)) {
    leader_info.highest_scoring_other_bid = bid_value;
    leader_info.highest_scoring_other_bid_in_seller_currency =
        bid_in_seller_currency;
  }
}

std::optional<base::TimeDelta> InterestGroupAuction::SellerTimeout() {
  if (config_->non_shared_params.seller_timeout.has_value()) {
    return std::min(config_->non_shared_params.seller_timeout.value(),
                    kMaxPerBuyerTimeout);
  }
  return std::nullopt;
}

void InterestGroupAuction::MaybeCompleteBiddingAndScoringPhase() {
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  if (!IsBiddingAndScoringPhaseComplete()) {
    return;
  }

  all_bids_scored_ = true;

  AuctionResult result = AuctionResult::kSuccess;
  if (is_server_auction_) {
    result = saved_response_->result;
  }

  // If there's no winning bid, fail with kAllBidsRejected if there were any
  // bids. Otherwise, fail with kNoBids or kNoInterestGroups.
  if (result == AuctionResult::kSuccess && !top_bid()) {
    if (any_bid_made_) {
      result = AuctionResult::kAllBidsRejected;
    } else {
      result = HasInterestGroups() ? AuctionResult::kNoBids
                                   : AuctionResult::kNoInterestGroups;
    }
  }

  // This needs to be asynchronous since it can happen inside
  // StartBiddingAndScoringPhase if there is actually nothing to do, and we
  // don't want to delete the auction while we're in process of starting it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InterestGroupAuction::OnBiddingAndScoringComplete,
                     weak_ptr_factory_.GetWeakPtr(), result,
                     std::vector<std::string>()));
}

void InterestGroupAuction::OnBiddingAndScoringComplete(
    AuctionResult auction_result,
    const std::vector<std::string>& errors) {
  DCHECK(bidding_and_scoring_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK_EQ(bidding_and_scoring_phase_state_, PhaseState::kDuring);
  bidding_and_scoring_phase_state_ = PhaseState::kAfter;

  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidding_and_scoring_phase",
                                  *trace_id_);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // If this is a component auction, have to unload the seller worklet handle
  // to avoid deadlock. Otherwise, loading the top-level seller worklet may be
  // blocked by component seller worklets taking up all the quota.
  if (parent_) {
    seller_worklet_handle_.reset();
  }

  // If the seller loaded callback hasn't been invoked yet, call it now. This
  // is needed in the case the phase ended without receiving the seller
  // worklet (e.g., in the case no bidder worklet bids).
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
    // depend on whether or not they won the top-level auction, so if this is
    // a component auction, leave status as-is.
    if (!parent_) {
      final_auction_result_ = AuctionResult::kSuccess;
      // If there's a winning bid, set its auction result as well. If the
      // winning bid came from a component auction, this will set that
      // component auction's result as well. This is needed for auction result
      // accessors.
      //
      // TODO(crbug.com/40248758): This is currently needed to
      // correctly retrieve reporting information from the nested auction. Is
      // there a cleaner way to do this?
      if (top_bid()) {
        top_bid()->bid->auction->final_auction_result_ =
            AuctionResult::kSuccess;
      }
    }
  }

  // If this is a top-level auction with component auction, update final state
  // of all successfully completed component auctions with bids that did not
  // win to reflect a loss.
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
  DCHECK(!bid_state.additional_bid_buyer);

  const blink::InterestGroup& interest_group = bid_state.bidder->interest_group;

  std::optional<uint16_t> experiment_group_id =
      GetBuyerExperimentId(*config_, interest_group.owner);

  return AuctionWorkletManager::BidderWorkletKey(
      interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url,
      /*needs_cors_for_additional_bid=*/false, experiment_group_id,
      GetTrustedBiddingSignalsSlotSizeParam(
          interest_group.trusted_bidding_signals_slot_size_mode),
      interest_group.trusted_bidding_signals_coordinator);
}

const std::string& InterestGroupAuction::GetTrustedBiddingSignalsSlotSizeParam(
    blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode
        trusted_bidding_signals_slot_size_mode) {
  auto it = trusted_bidding_signals_size_mode_strings_.find(
      trusted_bidding_signals_slot_size_mode);
  if (it != trusted_bidding_signals_size_mode_strings_.end()) {
    return it->second;
  }

  auto inserted = trusted_bidding_signals_size_mode_strings_.insert(
      std::pair(trusted_bidding_signals_slot_size_mode,
                CreateTrustedBiddingSignalsSlotSizeParam(
                    *config_, trusted_bidding_signals_slot_size_mode)));
  DCHECK(inserted.second);
  return inserted.first->second;
}

void InterestGroupAuction::OnDecompressedServerResponse(
    AdAuctionRequestContext* request_context,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  data_decoder::DataDecoder* data_decoder =
      get_data_decoder_callback_.Run(config_->seller);

  if (!result.has_value() || !data_decoder) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        data_decoder ? InvalidServerResponseReason::kDecompressFailure
                     : InvalidServerResponseReason::kDecoderShutdown);
    errors_.push_back("runAdAuction(): Could not decompress server response");
    if (bidding_and_scoring_phase_state_ == PhaseState::kDuring) {
      MaybeCompleteBiddingAndScoringPhase();
    }
    return;
  }
  data_decoder->ParseCbor(
      result.value(),
      base::BindOnce(
          &InterestGroupAuction::OnParsedServerResponse,
          weak_ptr_factory_.GetWeakPtr(),
          // This use of unretained is safe since the request_context is owned
          // by a PageUserData and is only deleted when the page is destroyed.
          // This auction is owned by the page through a DocumentService, so
          // it will be destroyed before the decoder.
          base::Unretained(request_context)));

  // `result` falls out of scope, deallocating the buffer with the
  // decompressed response. This is okay because `DataDecoder::ParseCbor` made
  // a copy before it returned (via implicit construction of
  // mojo_base::BigBuffer).
}

void InterestGroupAuction::OnParsedServerResponse(
    AdAuctionRequestContext* request_context,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!OnParsedServerResponseImpl(request_context, std::move(result))) {
    DCHECK(saved_response_);

    if (bidding_and_scoring_phase_state_ == PhaseState::kDuring) {
      MaybeCompleteBiddingAndScoringPhase();
    }
  }
}

bool InterestGroupAuction::OnParsedServerResponseImpl(
    AdAuctionRequestContext* request_context,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    // We couldn't parse the CBOR of the response.
    errors_.push_back("runAdAuction(): Could not parse server response");
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kCBORParseFailure);
    return false;
  }
  std::optional<BiddingAndAuctionResponse> response =
      BiddingAndAuctionResponse::TryParse(
          std::move(result).value(), request_context->group_names,
          request_context->group_pagg_coordinators);
  if (!response) {
    // We couldn't recognize the structure of the response.
    errors_.push_back(
        "runAdAuction(): Could not parse server response structure");
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kUnexpectedResponseStructure);
    return false;
  }
  if (response->error) {
    errors_.push_back(base::StrCat({"runAdAuction(): ", *response->error}));
  }

  if (response->is_chaff) {
    // This is a place-holder response because there was no winner.
    saved_response_.emplace();
    saved_response_->result = AuctionResult::kNoBids;
    return false;
  }

  if (parent_) {
    // This is a multi-level auction.
    if (!response->top_level_seller) {
      // but the response was for a single level auction. Return an error.
      errors_.push_back(
          "runAdAuction(): got server response for top-level auction in "
          "multi-level auction.");
      saved_response_.emplace();
      base::UmaHistogramEnumeration(
          kInvalidServerResponseReasonUMAName,
          InvalidServerResponseReason::kNotMultilevel);
      return false;
    } else if (parent_->config_->seller != response->top_level_seller.value()) {
      errors_.push_back(base::StrCat(
          {"runAdAuction(): got server response for toplevel seller `",
           response->top_level_seller->Serialize(),
           "`, expected top level seller `", config_->seller.Serialize(),
           "`."}));
      saved_response_.emplace();
      base::UmaHistogramEnumeration(
          kInvalidServerResponseReasonUMAName,
          InvalidServerResponseReason::kTopLevelSellerMismatch);
      return false;
    }
  } else if (response->top_level_seller) {
    // This is a single-level auction, but the response was for a multi-level
    // auction.
    errors_.push_back(
        "runAdAuction(): got server response for multi-level auction in "
        "single-level auction.");
    saved_response_.emplace();
    base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                  InvalidServerResponseReason::kNotSingleLevel);
    return false;
  }

  if (response->bid && !IsValidBid(*response->bid)) {
    errors_.push_back(base::StrCat({"runAdAuction(): Invalid bid value ",
                                    base::NumberToString(*response->bid)}));
    saved_response_.emplace();
    base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                  InvalidServerResponseReason::kInvalidBid);
    return false;
  }
  any_bid_made_ = !response->bidding_groups.empty();

  blink::InterestGroupKey winning_group(response->interest_group_owner,
                                        response->interest_group_name);
  // Winning group must be a bidder.
  if (!base::Contains(response->bidding_groups, winning_group)) {
    errors_.push_back("runAdAuction(): Winning group must be a bidder");
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kWinningGroupNotBidder);
    return false;
  }

  interest_group_manager_->GetInterestGroup(
      winning_group, base::BindOnce(&InterestGroupAuction::OnLoadedWinningGroup,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(response).value()));
  return true;
}

void InterestGroupAuction::OnLoadedWinningGroup(
    BiddingAndAuctionResponse response,
    std::optional<SingleStorageInterestGroup> maybe_group) {
  OnLoadedWinningGroupImpl(std::move(response), std::move(maybe_group));
  DCHECK(saved_response_);

  MaybeLoadDebugReportLockoutAndCooldowns();
}

void InterestGroupAuction::OnLoadedWinningGroupImpl(
    BiddingAndAuctionResponse response,
    std::optional<SingleStorageInterestGroup> maybe_group) {
  if (!maybe_group) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kMissingWinningGroup);
    errors_.emplace_back(
        "runAdAuction(): Could not load winning interest group");
    return;
  }

  if (!maybe_group.value()->interest_group.bidding_url) {
    // Groups must have a bidding logic URL to bid.
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kMissingWinningGroupBidURL);
    errors_.emplace_back(
        "runAdAuction(): Winning group doesn't have a bidding URL");
    return;
  }

  std::vector<SingleStorageInterestGroup> groups;
  groups.push_back(std::move(*maybe_group));
  auto buyer_helper = std::make_unique<BuyerHelper>(this, std::move(groups));
  buyer_helpers_.emplace_back(std::move(buyer_helper));

  for (const auto& [group_key, update_if_older_than] :
       response.triggered_updates) {
    HandleUpdateIfOlderThan(group_key.owner, group_key.name,
                            update_if_older_than);
  }

  response.result = AuctionResult::kSuccess;
  saved_response_ = std::move(response);
}

void InterestGroupAuction::MaybeLoadDebugReportLockoutAndCooldowns() {
  if (base::FeatureList::IsEnabled(
          blink::features::kBiddingAndScoringDebugReportingAPI) &&
      base::FeatureList::IsEnabled(
          blink::features::kFledgeSampleDebugReports) &&
      !server_auction_debug_report_lockout_loaded_) {
    // All ad tech origins that have debug reports.
    base::flat_set<url::Origin> origins =
        saved_response_->debugging_only_report_origins;
    // Use a weak pointer here so that
    // &InterestGroupAuction::OnLoadDebugReportLockoutAndCooldownsComplete is
    // cancelled when |weak_ptr_factory_| is destroyed.
    interest_group_manager_->GetDebugReportLockoutAndCooldowns(
        std::move(origins),
        base::BindOnce(
            &InterestGroupAuction::OnLoadDebugReportLockoutAndCooldownsComplete,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnLoadDebugReportLockoutAndCooldownsComplete(
        /*debug_report_lockout_and_cooldowns=*/std::nullopt);
  }
}

void InterestGroupAuction::OnLoadDebugReportLockoutAndCooldownsComplete(
    std::optional<DebugReportLockoutAndCooldowns>
        debug_report_lockout_and_cooldowns) {
  if (!server_auction_debug_report_lockout_loaded_) {
    debug_report_lockout_and_cooldowns_ =
        std::move(debug_report_lockout_and_cooldowns);
    server_auction_debug_report_lockout_loaded_ = true;
  }

  if (saved_response_ &&
      bidding_and_scoring_phase_state_ == PhaseState::kDuring &&
      !started_creating_bid_from_response_) {
    started_creating_bid_from_response_ = true;
    CreateBidFromServerResponse();
    num_scoring_dependencies_ = 0;
    MaybeCompleteBiddingAndScoringPhase();
  }
}

void InterestGroupAuction::CreateBidFromServerResponse() {
  DCHECK(saved_response_);
  DCHECK_EQ(PhaseState::kDuring, bidding_and_scoring_phase_state_);

  if (saved_response_->result != AuctionResult::kSuccess) {
    return;
  }
  // We require a bid for component auctions. Otherwise we use a fake value.
  if (parent_ && !saved_response_->bid) {
    saved_response_->result = AuctionResult::kInvalidServerResponse;
    errors_.emplace_back("runAdAuction(): Missing bid value");
    return;
  }

  CHECK_EQ(1u, buyer_helpers_.size());

  std::vector<blink::AdDescriptor> ad_components;
  base::ranges::transform(
      saved_response_->ad_components, std::back_inserter(ad_components),
      [](const GURL& url) { return blink::AdDescriptor(url); });
  std::unique_ptr<Bid> bid =
      buyer_helpers_[0]->TryToCreateBidFromServerResponse(
          auction_worklet::mojom::BidRole::kUnenforcedKAnon,
          saved_response_->bid.value_or(0.0001), saved_response_->bid_currency,
          saved_response_->ad_metadata, saved_response_->buyer_reporting_id,
          saved_response_->buyer_and_seller_reporting_id,
          /*ad_descriptor=*/
          blink::AdDescriptor(saved_response_->ad_render_url),
          /*ad_component_descriptors=*/std::move(ad_components),
          saved_response_->component_win_pagg_requests,
          saved_response_->server_filtered_pagg_requests_reserved,
          saved_response_->server_filtered_pagg_requests_non_reserved,
          saved_response_->component_win_debugging_only_reports,
          saved_response_->server_filtered_debugging_only_reports);

  if (!bid) {
    saved_response_.emplace();
    base::UmaHistogramEnumeration(
        kInvalidServerResponseReasonUMAName,
        InvalidServerResponseReason::kConstructBidFailure);
    errors_.emplace_back("runAdAuction(): Couldn't reconstruct winning bid");
    return;
  }

  auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr
      component_auction_modified_bid_params;
  if (parent_) {
    if (!blink::VerifyAdCurrencyCode(config_->non_shared_params.seller_currency,
                                     saved_response_->bid_currency)) {
      saved_response_.emplace();
      base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                    InvalidServerResponseReason::kBadCurrency);
      errors_.emplace_back(
          "runAdAuction(): currency didn't match auction config");
      return;
    }
    if (!blink::VerifyAdCurrencyCode(
            PerBuyerCurrency(config_->seller, *parent_->config_),
            saved_response_->bid_currency)) {
      saved_response_.emplace();
      base::UmaHistogramEnumeration(kInvalidServerResponseReasonUMAName,
                                    InvalidServerResponseReason::kBadCurrency);
      errors_.emplace_back(
          "runAdAuction(): currency didn't match top-level per-buyer currency");
      return;
    }

    // Component auction.
    component_auction_modified_bid_params =
        auction_worklet::mojom::ComponentAuctionModifiedBidParams::New();
    component_auction_modified_bid_params->ad = bid->ad_metadata;
  }

  // TODO(behamilton): Refactor this once B&A supports k-anonymity. For now we
  // treat all bids from B&A as k-anonymous.
  bid->bid_role = auction_worklet::mojom::BidRole::kBothKAnonModes;
  auto bid_copy = std::make_unique<Bid>(*bid);
  auto modified_bid_params_copy =
      component_auction_modified_bid_params
          ? component_auction_modified_bid_params->Clone()
          : auction_worklet::mojom::ComponentAuctionModifiedBidParamsPtr();
  UpdateAuctionLeaders(std::move(bid), saved_response_->score.value_or(0.00001),
                       std::move(component_auction_modified_bid_params),
                       /*bid_in_seller_currency=*/std::nullopt,
                       /*scoring_signals_data_version=*/std::nullopt,
                       non_kanon_enforced_auction_leader_);
  UpdateAuctionLeaders(std::move(bid_copy),
                       saved_response_->score.value_or(0.00001),
                       std::move(modified_bid_params_copy),
                       /*bid_in_seller_currency=*/std::nullopt,
                       /*scoring_signals_data_version=*/std::nullopt,
                       kanon_enforced_auction_leader_);
}

void InterestGroupAuction::OnDirectFromSellerSignalHeaderAdSlotResolved(
    std::string ad_slot,
    scoped_refptr<HeaderDirectFromSellerSignals::Result> signals) {
  DCHECK_NE(bidding_and_scoring_phase_state_, PhaseState::kAfter);
  CHECK(direct_from_seller_signals_header_ad_slot_pending_);
  CHECK(direct_from_seller_signals_header_ad_slot_);
  if (signals) {
    direct_from_seller_signals_header_ad_slot_ = std::move(signals);
  } else {
    errors_.push_back(DirectFromSellerSignalsHeaderAdSlotNoMatchError(ad_slot));
  }

  direct_from_seller_signals_header_ad_slot_pending_ = false;

  if (bidding_and_scoring_phase_state_ == PhaseState::kDuring) {
    for (const auto& buyer_helper : buyer_helpers_) {
      buyer_helper->NotifyConfigDependencyResolved();
    }
    OnScoringDependencyDone();
    ScoreQueuedBidsIfReady();
  }
}

void InterestGroupAuction::UpdateIgSizeMetrics(
    const std::vector<SingleStorageInterestGroup>& interest_groups) {
  for (const SingleStorageInterestGroup& group : interest_groups) {
    interest_groups_bytes_for_metrics_ += group->interest_group.EstimateSize();

    if (group->interest_group.ads) {
      for (const blink::InterestGroup::Ad& ad : *group->interest_group.ads) {
        ads_and_ad_components_bytes_for_metrics_ += ad.EstimateSize();
      }
    }

    if (group->interest_group.ad_components) {
      for (const blink::InterestGroup::Ad& ad_components :
           *group->interest_group.ad_components) {
        ads_and_ad_components_bytes_for_metrics_ +=
            ad_components.EstimateSize();
      }
    }
  }
}

}  // namespace content
