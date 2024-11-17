// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_auction_reporter.h"

#include <stdint.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/noiser_and_bucketer.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/privacy_sandbox_invoking_api.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-shared.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// All event-level reporting URLs received from worklets must be valid HTTPS
// URLs. It's up to callers to call ReportBadMessage() on invalid URLs.
bool IsEventLevelReportingUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

const blink::InterestGroup::Ad& ChosenAd(
    const SingleStorageInterestGroup& storage_interest_group,
    const GURL& winning_ad_url) {
  auto chosen_ad = base::ranges::find(
      *storage_interest_group->interest_group.ads, winning_ad_url,
      [](const blink::InterestGroup::Ad& ad) { return ad.render_url(); });
  CHECK(chosen_ad != storage_interest_group->interest_group.ads->end());
  return *chosen_ad;
}

bool IsKAnonForReporting(
    const SingleStorageInterestGroup& storage_interest_group,
    const blink::InterestGroup::Ad& chosen_ad,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id) {
  // K-anonymity enforcement is always disabled for the testing population.
  if (base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(
          blink::features::kFledgeConsiderKAnonymity) ||
      !base::FeatureList::IsEnabled(
          blink::features::kFledgeEnforceKAnonymity)) {
    return true;
  }

  std::string reporting_key = HashedKAnonKeyForAdNameReporting(
      storage_interest_group->interest_group, chosen_ad,
      selected_buyer_and_seller_reporting_id);
  return storage_interest_group->hashed_kanon_keys.contains(reporting_key);
}

// Sets the reporting IDs to pass to the worklet's `reportWin()` method,
// based on the reporting ID fields present in the winning ad.
//
// `reporting_id_field` will be set to the field that's populated, and the
// corresponding output parameter will be populated with the reporting ID
// from the winning ad.
//
// The order of precedence for which reporting ID is sent is:
//   1. `selected_buyer_and_seller_reporting_id` (more context below)
//   2. `buyer_and_seller_reporting_id`
//   3. `buyer_reporting_id`
//   4. `interest_group_name` (Only if none of the above are present)
//
// When `selected_buyer_and_seller_reporting_id` is set in the winning ad,
// the `buyer_reporting_id` and `buyer_and_seller_reporting_id` fields may
// also be populated.
//
// If `selected_buyer_and_seller_reporting_id` is not set, then only one
// reporting ID field will be populated.
void SetReportWinReportingIds(
    const std::string& interest_group_name,
    const std::optional<std::string>&
        maybe_selected_buyer_and_seller_reporting_id,
    const blink::InterestGroup::Ad& chosen_ad,
    auction_worklet::mojom::ReportingIdField& reporting_id_field,
    std::optional<std::string>& interest_group_name_reporting_id,
    std::optional<std::string>& buyer_reporting_id,
    std::optional<std::string>& buyer_and_seller_reporting_id,
    std::optional<std::string>& selected_buyer_and_seller_reporting_id) {
  if (maybe_selected_buyer_and_seller_reporting_id.has_value()) {
    reporting_id_field = auction_worklet::mojom::ReportingIdField::
        kSelectedBuyerAndSellerReportingId;
    selected_buyer_and_seller_reporting_id =
        maybe_selected_buyer_and_seller_reporting_id;
    buyer_and_seller_reporting_id = chosen_ad.buyer_and_seller_reporting_id;
    buyer_reporting_id = chosen_ad.buyer_reporting_id;
    return;
  }
  if (chosen_ad.buyer_and_seller_reporting_id.has_value()) {
    reporting_id_field =
        auction_worklet::mojom::ReportingIdField::kBuyerAndSellerReportingId;
    buyer_and_seller_reporting_id = *chosen_ad.buyer_and_seller_reporting_id;
    return;
  }
  if (chosen_ad.buyer_reporting_id.has_value()) {
    reporting_id_field =
        auction_worklet::mojom::ReportingIdField::kBuyerReportingId;
    buyer_reporting_id = *chosen_ad.buyer_reporting_id;
    return;
  }
  reporting_id_field =
      auction_worklet::mojom::ReportingIdField::kInterestGroupName;
  interest_group_name_reporting_id = interest_group_name;
}

// If any of private aggregation request is wrong, calls ReportBadMessage and
// returns false.
bool ValidateReportingPrivateAggregationRequests(
    const PrivateAggregationRequests& pa_requests) {
  std::optional<std::string> error =
      content::ValidatePrivateAggregationRequests(pa_requests);
  if (error.has_value()) {
    mojo::ReportBadMessage(*error);
    return false;
  }

  for (const auto& request : pa_requests) {
    if (IsPrivateAggregationRequestReservedOnce(*request)) {
      mojo::ReportBadMessage(
          "Reporting Private Aggregation request using reserved.once");
      return false;
    }
  }

  return true;
}

}  // namespace

BASE_FEATURE(kFledgeRounding,
             "FledgeRounding",
             base::FEATURE_ENABLED_BY_DEFAULT);
// For now default bid and score to full resolution.
const base::FeatureParam<int> kFledgeBidReportingBits{
    &kFledgeRounding, "fledge_bid_reporting_bits", 8};
const base::FeatureParam<int> kFledgeScoreReportingBits{
    &kFledgeRounding, "fledge_score_reporting_bits", 8};
const base::FeatureParam<int> kFledgeAdCostReportingBits{
    &kFledgeRounding, "fledge_ad_cost_reporting_bits", 8};

InterestGroupAuctionReporter::SellerWinningBidInfo::SellerWinningBidInfo() =
    default;
InterestGroupAuctionReporter::SellerWinningBidInfo::SellerWinningBidInfo(
    SellerWinningBidInfo&&) = default;
InterestGroupAuctionReporter::SellerWinningBidInfo::~SellerWinningBidInfo() =
    default;
InterestGroupAuctionReporter::SellerWinningBidInfo&
InterestGroupAuctionReporter::SellerWinningBidInfo::operator=(
    SellerWinningBidInfo&&) = default;

InterestGroupAuctionReporter::WinningBidInfo::WinningBidInfo(
    const SingleStorageInterestGroup& storage_interest_group)
    : storage_interest_group(std::move(storage_interest_group)) {}
InterestGroupAuctionReporter::WinningBidInfo::WinningBidInfo(WinningBidInfo&&) =
    default;
InterestGroupAuctionReporter::WinningBidInfo::~WinningBidInfo() = default;

InterestGroupAuctionReporter::InterestGroupAuctionReporter(
    InterestGroupManagerImpl* interest_group_manager,
    AuctionWorkletManager* auction_worklet_manager,
    BrowserContext* browser_context,
    PrivateAggregationManager* private_aggregation_manager,
    LogPrivateAggregationRequestsCallback
        log_private_aggregation_requests_callback,
    AdAuctionPageDataCallback ad_auction_page_data_callback,
    std::unique_ptr<blink::AuctionConfig> auction_config,
    const std::string& devtools_auction_id,
    const url::Origin& main_frame_origin,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    auction_worklet::mojom::KAnonymityBidMode kanon_mode,
    bool bid_is_kanon,
    WinningBidInfo winning_bid_info,
    SellerWinningBidInfo top_level_seller_winning_bid_info,
    std::optional<SellerWinningBidInfo> component_seller_winning_bid_info,
    blink::InterestGroupSet interest_groups_that_bid,
    std::vector<GURL> debug_win_report_urls,
    std::vector<GURL> debug_loss_report_urls,
    base::flat_set<std::string> k_anon_keys_to_join,
    std::map<PrivateAggregationKey, PrivateAggregationRequests>
        private_aggregation_requests_reserved,
    std::map<std::string, PrivateAggregationRequests>
        private_aggregation_requests_non_reserved,
    PrivateAggregationAllParticipantsData all_participants_data,
    std::map<url::Origin, RealTimeReportingContributions>
        real_time_contributions)
    : interest_group_manager_(interest_group_manager),
      auction_worklet_manager_(auction_worklet_manager),
      private_aggregation_manager_(private_aggregation_manager),
      log_private_aggregation_requests_callback_(
          std::move(log_private_aggregation_requests_callback)),
      ad_auction_page_data_callback_(std::move(ad_auction_page_data_callback)),
      auction_config_(std::move(auction_config)),
      devtools_auction_id_(devtools_auction_id),
      main_frame_origin_(main_frame_origin),
      frame_origin_(frame_origin),
      client_security_state_(std::move(client_security_state)),
      url_loader_factory_(std::move(url_loader_factory)),
      kanon_mode_(kanon_mode),
      bid_is_kanon_(bid_is_kanon),
      winning_bid_info_(std::move(winning_bid_info)),
      top_level_seller_winning_bid_info_(
          std::move(top_level_seller_winning_bid_info)),
      component_seller_winning_bid_info_(
          std::move(component_seller_winning_bid_info)),
      interest_groups_that_bid_(std::move(interest_groups_that_bid)),
      debug_win_report_urls_(std::move(debug_win_report_urls)),
      debug_loss_report_urls_(std::move(debug_loss_report_urls)),
      k_anon_keys_to_join_(std::move(k_anon_keys_to_join)),
      private_aggregation_requests_reserved_(
          std::move(private_aggregation_requests_reserved)),
      private_aggregation_requests_non_reserved_(
          std::move(private_aggregation_requests_non_reserved)),
      all_participants_data_(std::move(all_participants_data)),
      real_time_contributions_(std::move(real_time_contributions)),
      fenced_frame_reporter_(FencedFrameReporter::CreateForFledge(
          url_loader_factory_,
          browser_context,
          /*direct_seller_is_seller=*/
          !component_seller_winning_bid_info_.has_value(),
          private_aggregation_manager_,
          main_frame_origin_,
          winning_bid_info_.storage_interest_group->interest_group.owner,
          winning_bid_info_.storage_interest_group->interest_group
              .aggregation_coordinator_origin,
          winning_bid_info_.allowed_reporting_origins)),
      browser_context_(browser_context) {
  DCHECK(interest_group_manager_);
  DCHECK(auction_worklet_manager_);
  DCHECK(url_loader_factory_);
  DCHECK(client_security_state_);
  EnforceAttestationsReportUrls(debug_win_report_urls_);
  EnforceAttestationsReportUrls(debug_loss_report_urls_);
}

InterestGroupAuctionReporter ::~InterestGroupAuctionReporter() {
  base::UmaHistogramEnumeration("Ads.InterestGroup.Auction.FinalReporterState",
                                navigated_to_winning_ad_
                                    ? reporter_worklet_state_
                                    : ReporterState::kAdNotUsed);
}

void InterestGroupAuctionReporter::Start(base::OnceClosure callback) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "fledge", "reporting_phase", top_level_seller_winning_bid_info_.trace_id);

  DCHECK(!callback_);

  // If this is a single level auction, and there was no server-side component
  // seller, set the component seller mapping as empty, so it's available as
  // soon as possible.
  bool has_component_seller_reporting =
      component_seller_winning_bid_info_.has_value() ||
      (top_level_seller_winning_bid_info_.saved_response.has_value() &&
       top_level_seller_winning_bid_info_.saved_response
           ->component_seller_reporting.has_value());
  if (!has_component_seller_reporting) {
    fenced_frame_reporter_->OnUrlMappingReady(
        blink::FencedFrame::ReportingDestination::kComponentSeller,
        /*reporting_url_declarer_origin=*/std::nullopt,
        /*reporting_url_map=*/{});
  }
  callback_ = std::move(callback);

  if (reporter_worklet_state_ == ReporterState::kAllWorkletsCompleted) {
    OnReportingComplete();
    return;
  }
  RequestSellerWorklet(&top_level_seller_winning_bid_info_,
                       /*top_seller_signals=*/std::nullopt);
}

void InterestGroupAuctionReporter::InitializeFromServerResponse(
    const BiddingAndAuctionResponse& response,
    blink::FencedFrame::ReportingDestination seller_destination) {
  reporter_worklet_state_ = ReporterState::kAllWorkletsCompleted;

  if (seller_destination == blink::FencedFrame::ReportingDestination::kSeller) {
    if (response.top_level_seller_reporting) {
      const BiddingAndAuctionResponse::ReportingURLs& seller_reporting =
          *response.top_level_seller_reporting;
      // Ignore return value - there's nothing we can do at this point if the
      // server did something wrong beyond logging the error.
      AddReportResultResult(
          auction_config_->seller, seller_reporting.reporting_url,
          seller_reporting.beacon_urls, seller_destination, errors_);
    }
    // Support for server-orchestrated multi-level auctions. They will also have
    // component_seller_reporting.
    if (response.component_seller_reporting) {
      const BiddingAndAuctionResponse::ReportingURLs& seller_reporting =
          *response.component_seller_reporting;
      // Ignore return value - there's nothing we can do at this point if the
      // server did something wrong beyond logging the error.
      AddReportResultResult(
          auction_config_->seller, seller_reporting.reporting_url,
          seller_reporting.beacon_urls,
          blink::FencedFrame::ReportingDestination::kComponentSeller, errors_);
    }
  } else {
    DCHECK_EQ(blink::FencedFrame::ReportingDestination::kComponentSeller,
              seller_destination);
    if (response.component_seller_reporting) {
      const BiddingAndAuctionResponse::ReportingURLs& seller_reporting =
          *response.component_seller_reporting;
      // Ignore return value - there's nothing we can do at this point if the
      // server did something wrong beyond logging the error.
      AddReportResultResult(
          auction_config_->seller, seller_reporting.reporting_url,
          seller_reporting.beacon_urls, seller_destination, errors_);
    }
  }
  if (response.buyer_reporting) {
    const BiddingAndAuctionResponse::ReportingURLs& buyer_reporting =
        *response.buyer_reporting;
    // Ignore return value - there's nothing we can do at this point if the
    // server did something wrong beyond logging the error.
    AddReportWinResult(response.interest_group_owner,
                       buyer_reporting.reporting_url,
                       buyer_reporting.beacon_urls,
                       /*bidder_ad_macro_map=*/std::nullopt, errors_);
  }
}

base::RepeatingClosure
InterestGroupAuctionReporter::OnNavigateToWinningAdCallback(
    FrameTreeNodeId frame_tree_node_id) {
  return base::BindRepeating(
      &InterestGroupAuctionReporter::OnNavigateToWinningAd,
      weak_ptr_factory_.GetWeakPtr(), frame_tree_node_id);
}

void InterestGroupAuctionReporter::OnFledgePrivateAggregationRequests(
    PrivateAggregationManager* private_aggregation_manager,
    const url::Origin& main_frame_origin,
    std::map<PrivateAggregationKey,
             std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>>
        private_aggregation_requests) {
  // Empty vectors should've been filtered out.
  DCHECK(base::ranges::none_of(private_aggregation_requests,
                               [](auto& it) { return it.second.empty(); }));

  if (private_aggregation_requests.empty() || !private_aggregation_manager) {
    return;
  }

  for (auto& [agg_key, requests] : private_aggregation_requests) {
    SplitContributionsIntoBatchesThenSendToHost(
        std::move(requests), *private_aggregation_manager,
        /*reporting_origin=*/agg_key.reporting_origin,
        std::move(agg_key.aggregation_coordinator_origin), main_frame_origin);
  }
}

/* static */
double InterestGroupAuctionReporter::RoundBidStochastically(double bid) {
  return RoundStochasticallyToKBits(bid, kFledgeBidReportingBits.Get());
}

/* static */
double InterestGroupAuctionReporter::RoundStochasticallyToKBits(double value,
                                                                unsigned k) {
  int value_exp;
  if (!std::isfinite(value)) {
    return value;
  }

  double norm_value = std::frexp(value, &value_exp);
  // frexp() returns numbers in the range +-[0.5, 1)

  if (value_exp < std::numeric_limits<int8_t>::min()) {
    return std::copysign(0, value);
  }
  if (value_exp > std::numeric_limits<int8_t>::max()) {
    return std::copysign(std::numeric_limits<double>::infinity(), value);
  }

  // Shift so we get k integer bits. Since we are in the range +-[0.5, 1) we
  // multiply by 2**k to get to the range +-[2**(k-1), 2**k).
  double precision_scaled_value = std::ldexp(norm_value, k);

  // Remove the fractional part.
  double truncated_scaled_value = std::trunc(precision_scaled_value);

  // Apply random noise based on truncated portion such that we increment with
  // probability equal to the truncated portion.
  double noised_truncated_scaled_value = truncated_scaled_value;
  if (std::abs(precision_scaled_value - truncated_scaled_value) >
      base::RandDouble()) {
    noised_truncated_scaled_value =
        truncated_scaled_value + std::copysign(1, precision_scaled_value);

    // Handle overflow caused by the increment. Incrementing can only
    // increase the absolute value, so only worry about the mantissa
    // overflowing.
    if (value_exp == std::numeric_limits<int8_t>::max() &&
        std::abs(std::ldexp(noised_truncated_scaled_value, -k)) >= 1.0) {
      DCHECK_EQ(1.0, std::abs(std::ldexp(noised_truncated_scaled_value, -k)));
      return std::copysign(std::numeric_limits<double>::infinity(), value);
    }
  }

  return std::ldexp(noised_truncated_scaled_value, value_exp - k);
}

/* static */
std::optional<double> InterestGroupAuctionReporter::RoundStochasticallyToKBits(
    std::optional<double> maybe_value,
    unsigned k) {
  if (!maybe_value.has_value()) {
    return std::nullopt;
  }
  return RoundStochasticallyToKBits(*maybe_value, k);
}

void InterestGroupAuctionReporter::RequestSellerWorklet(
    const SellerWinningBidInfo* seller_info,
    const std::optional<std::string>& top_seller_signals) {
  if (seller_info == &top_level_seller_winning_bid_info_) {
    reporter_worklet_state_ = ReporterState::kSellerReportResult;
  } else {
    reporter_worklet_state_ = ReporterState::kComponentSellerReportResult;
  }
  seller_worklet_handle_.reset();

  if (seller_info->saved_response.has_value()) {
    InitializeFromServerResponse(
        *seller_info->saved_response,
        seller_info == &top_level_seller_winning_bid_info_
            ? blink::FencedFrame::ReportingDestination::kSeller
            : blink::FencedFrame::ReportingDestination::kComponentSeller);

    // If any event-level reports were queued, send them now, if the winning ad
    // has been navigated to.
    SendPendingReportsIfNavigated();

    OnReportingComplete();
    return;
  }
  // base::Unretained is safe to use for these callbacks because destroying
  // `seller_worklet_handle_` will prevent the callbacks from being invoked, if
  // `this` is destroyed while still waiting on the callbacks.
  auction_worklet_manager_->RequestSellerWorklet(
      devtools_auction_id_, *seller_info->auction_config->decision_logic_url,
      seller_info->auction_config->trusted_scoring_signals_url,
      seller_info->auction_config->seller_experiment_group_id,
      seller_info->auction_config->non_shared_params
          .trusted_scoring_signals_coordinator,
      base::BindOnce(&InterestGroupAuctionReporter::OnSellerWorkletReceived,
                     base::Unretained(this), base::Unretained(seller_info),
                     top_seller_signals),
      base::BindOnce(&InterestGroupAuctionReporter::OnSellerWorkletFatalError,
                     base::Unretained(this), base::Unretained(seller_info)),
      seller_worklet_handle_, /*auction_metrics_recorder=*/nullptr);
}

void InterestGroupAuctionReporter::OnSellerWorkletFatalError(
    const SellerWinningBidInfo* seller_info,
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  // On a seller load failure or crash, act as if the worklet returned no
  // results to advance to the next worklet.
  OnSellerReportResultComplete(
      seller_info,
      /*winning_bid=*/0.0,
      /*highest_scoring_other_bid=*/0.0,
      /*signals_for_winner=*/std::nullopt,
      /*seller_report_url=*/std::nullopt,
      /*seller_ad_beacon_map=*/{},
      /*pa_requests=*/{},
      /*timing_metrics=*/auction_worklet::mojom::SellerTimingMetrics::New(),
      errors);
}

void InterestGroupAuctionReporter::OnSellerWorkletReceived(
    const SellerWinningBidInfo* seller_info,
    const std::optional<std::string>& top_seller_signals) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("fledge", "seller_worklet_report_result",
                                    seller_info->trace_id);

  auction_worklet::mojom::ComponentAuctionOtherSellerPtr other_seller;
  auction_worklet::mojom::ComponentAuctionReportResultParamsPtr
      browser_signals_component_auction_report_result_params;
  bool top_level_with_components = false;
  if (seller_info == &top_level_seller_winning_bid_info_) {
    DCHECK(!top_seller_signals);
    if (component_seller_winning_bid_info_) {
      top_level_with_components = true;
      other_seller = auction_worklet::mojom::ComponentAuctionOtherSeller::
          NewComponentSeller(
              component_seller_winning_bid_info_->auction_config->seller);
    }
  } else {
    DCHECK(top_seller_signals);
    DCHECK(component_seller_winning_bid_info_);
    DCHECK(seller_info->component_auction_modified_bid_params);
    std::optional<double> rounded_modified_bid;
    if (seller_info->component_auction_modified_bid_params->bid.has_value()) {
      rounded_modified_bid = top_level_seller_winning_bid_info_.rounded_bid;
    }
    other_seller =
        auction_worklet::mojom::ComponentAuctionOtherSeller::NewTopLevelSeller(
            top_level_seller_winning_bid_info_.auction_config->seller);
    browser_signals_component_auction_report_result_params =
        auction_worklet::mojom::ComponentAuctionReportResultParams::New(
            /*top_level_seller_signals=*/std::move(top_seller_signals).value(),
            /*modified_bid=*/
            rounded_modified_bid);
  }

  double bid = seller_info->bid;
  double winning_bid_for_aggregation = bid;
  std::optional<blink::AdCurrency> bid_currency = seller_info->bid_currency;
  std::optional<blink::AdCurrency> highest_scoring_other_bid_currency;
  double highest_scoring_other_bid = seller_info->highest_scoring_other_bid;
  if (seller_info->auction_config->non_shared_params.seller_currency
          .has_value()) {
    // While reportResult() gets the bid that actually participated in the
    // auction, for private aggregation and second-highest we want to do things
    // in seller currency if it's enabled.
    winning_bid_for_aggregation = seller_info->bid_in_seller_currency;
    highest_scoring_other_bid =
        seller_info->highest_scoring_other_bid_in_seller_currency.value_or(0.0);
    highest_scoring_other_bid_currency =
        *seller_info->auction_config->non_shared_params.seller_currency;
  }

  // top-level auctions with components don't report highest_scoring_other_bid.
  if (top_level_with_components) {
    highest_scoring_other_bid = 0.0;
    highest_scoring_other_bid_currency = std::nullopt;
  }

  // Send in buyer_and_seller_reporting_id and
  // selected_buyer_and_seller_reporting_id if it's configured on the winning
  // ad and sufficiently k-anonymous.
  std::optional<std::string> browser_signal_buyer_and_seller_reporting_id;
  std::optional<std::string>
      browser_signal_selected_buyer_and_seller_reporting_id;

  auto chosen_ad = ChosenAd(winning_bid_info_.storage_interest_group,
                            winning_bid_info_.render_url);
  if (IsKAnonForReporting(
          winning_bid_info_.storage_interest_group, chosen_ad,
          winning_bid_info_.selected_buyer_and_seller_reporting_id)) {
    browser_signal_buyer_and_seller_reporting_id =
        chosen_ad.buyer_and_seller_reporting_id;
    browser_signal_selected_buyer_and_seller_reporting_id =
        winning_bid_info_.selected_buyer_and_seller_reporting_id;
  }

  seller_worklet_handle_->AuthorizeSubresourceUrls(
      *seller_info->subresource_url_builder);
  seller_worklet_handle_->GetSellerWorklet()->ReportResult(
      seller_info->auction_config->non_shared_params,
      InterestGroupAuction::GetDirectFromSellerSellerSignals(
          seller_info->subresource_url_builder.get()),
      InterestGroupAuction::GetDirectFromSellerSellerSignalsHeaderAdSlot(
          *seller_info->direct_from_seller_signals_header_ad_slot),
      InterestGroupAuction::GetDirectFromSellerAuctionSignals(
          seller_info->subresource_url_builder.get()),
      InterestGroupAuction::GetDirectFromSellerAuctionSignalsHeaderAdSlot(
          *seller_info->direct_from_seller_signals_header_ad_slot),
      std::move(other_seller),
      winning_bid_info_.storage_interest_group->interest_group.owner,
      /*browser_signal_buyer_and_seller_reporting_id=*/
      browser_signal_buyer_and_seller_reporting_id,
      /*browser_signal_selected_buyer_and_seller_reporting_id=*/
      browser_signal_selected_buyer_and_seller_reporting_id,
      winning_bid_info_.render_url, seller_info->rounded_bid, bid_currency,
      RoundStochasticallyToKBits(seller_info->score,
                                 kFledgeScoreReportingBits.Get()),
      RoundStochasticallyToKBits(highest_scoring_other_bid,
                                 kFledgeBidReportingBits.Get()),
      highest_scoring_other_bid_currency,
      std::move(browser_signals_component_auction_report_result_params),
      seller_info->scoring_signals_data_version, seller_info->trace_id,
      base::BindOnce(
          &InterestGroupAuctionReporter::OnSellerReportResultComplete,
          weak_ptr_factory_.GetWeakPtr(), base::Unretained(seller_info),
          winning_bid_for_aggregation, highest_scoring_other_bid));
}

void InterestGroupAuctionReporter::OnSellerReportResultComplete(
    const SellerWinningBidInfo* seller_info,
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<std::string>& signals_for_winner,
    const std::optional<GURL>& seller_report_url,
    const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
    PrivateAggregationRequests pa_requests,
    auction_worklet::mojom::SellerTimingMetricsPtr timing_metrics,
    const std::vector<std::string>& errors) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "seller_worklet_report_result",
                                  seller_info->trace_id);
  seller_worklet_handle_.reset();

  log_private_aggregation_requests_callback_.Run(pa_requests);

  PrivateAggregationTimings timings;
  timings.script_run_time = timing_metrics->script_latency;

  PrivateAggregationParticipantData& participant_data =
      all_participants_data_[static_cast<size_t>(
          seller_info == &top_level_seller_winning_bid_info_
              ? PrivateAggregationPhase::kTopLevelSeller
              : PrivateAggregationPhase::kNonTopLevelSeller)];
  participant_data.average_code_fetch_time =
      timing_metrics->js_fetch_latency.value_or(base::TimeDelta());
  participant_data.percent_scripts_timeout =
      timing_metrics->script_timed_out ? 100 : 0;

  if (!ValidateReportingPrivateAggregationRequests(pa_requests)) {
    pa_requests.clear();
  }
  for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
       pa_requests) {
    // reportResult() only gets executed for seller when there was an auction
    // winner so we consider is_winner to be true, which results in
    // "reserved.loss" reports not being reported. Bid reject reason is not
    // meaningful thus not supported in reportResult(), so it is set to
    // std::nullopt.
    std::optional<PrivateAggregationRequestWithEventType> converted_request =
        FillInPrivateAggregationRequest(
            std::move(request), winning_bid, highest_scoring_other_bid,
            /*reject_reason=*/std::nullopt, participant_data, timings,
            /*is_winner=*/true);

    // Only private aggregation requests with reserved event types are kept for
    // seller.
    if (converted_request.has_value() &&
        !converted_request.value().event_type.has_value()) {
      PrivateAggregationKey agg_key = {
          seller_info->auction_config->seller,
          seller_info->auction_config->aggregation_coordinator_origin};
      private_aggregation_requests_reserved_[std::move(agg_key)].emplace_back(
          std::move(converted_request.value().request));
    }
  }

  blink::FencedFrame::ReportingDestination reporting_destination;
  if (seller_info == &top_level_seller_winning_bid_info_) {
    reporting_destination = blink::FencedFrame::ReportingDestination::kSeller;
  } else {
    reporting_destination =
        blink::FencedFrame::ReportingDestination::kComponentSeller;
  }

  std::vector<std::string> validation_errors;
  if (!AddReportResultResult(seller_info->auction_config->seller,
                             seller_report_url, seller_ad_beacon_map,
                             reporting_destination, validation_errors)) {
    for (const auto& error : validation_errors) {
      mojo::ReportBadMessage(error);
    }
  }
  // If any reports were queued (event-level or aggregated), send them now, if
  // the winning ad has been navigated to.
  SendPendingReportsIfNavigated();

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Treat a null `signals_for_winner` value as a null JS response.
  //
  // TODO(mmenke): Consider making `signals_for_winner` itself non-optional, and
  // clean this up.
  std::string fixed_up_signals_for_winner = signals_for_winner.value_or("null");

  // If a the winning bid is from a nested component auction, need to call into
  // that Auction's report logic (which will invoke both that seller's
  // ReportResult() method, and the bidder's ReportWin()).
  if (seller_info == &top_level_seller_winning_bid_info_ &&
      component_seller_winning_bid_info_) {
    RequestSellerWorklet(&component_seller_winning_bid_info_.value(),
                         fixed_up_signals_for_winner);
    return;
  }

  RequestBidderWorklet(fixed_up_signals_for_winner);
}

bool InterestGroupAuctionReporter::AddReportResultResult(
    const url::Origin& seller_origin,
    const std::optional<GURL>& seller_report_url,
    const base::flat_map<std::string, GURL>& seller_ad_beacon_map,
    blink::FencedFrame::ReportingDestination destination,
    std::vector<std::string>& errors_out) {
  // This will be cleared if any beacons are invalid.
  base::flat_map<std::string, GURL> validated_seller_ad_beacon_map =
      seller_ad_beacon_map;
  for (const auto& element : seller_ad_beacon_map) {
    if (!IsEventLevelReportingUrlValid(element.second)) {
      errors_out.push_back(base::StrCat(
          {"Invalid seller beacon URL for '", element.first, "'"}));
      // Drop the entire beacon map if part of it is invalid. No need to treat
      // the rest of the data received from the worklet as invalid - all fields
      // are validated and consumed independently, and it's not worth the
      // complexity to make sure everything is dropped when a field is invalid.
      validated_seller_ad_beacon_map.clear();
      break;
    }
  }
  fenced_frame_reporter_->OnUrlMappingReady(
      destination, seller_origin, std::move(validated_seller_ad_beacon_map));

  if (seller_report_url) {
    if (!IsEventLevelReportingUrlValid(*seller_report_url)) {
      errors_out.push_back("Invalid seller report URL");
      // No need to skip rest of work on failure - all fields are validated and
      // consumed independently, and it's not worth the complexity to make sure
      // everything is dropped when a field is invalid.
    } else {
      AddPendingReportUrl(*seller_report_url);
    }
  }

  return errors_out.empty();
}

void InterestGroupAuctionReporter::RequestBidderWorklet(
    const std::string& signals_for_winner) {
  // Seller worklets should have been unloaded by now, and bidder worklet should
  // not have been loaded yet.
  DCHECK(!seller_worklet_handle_);
  DCHECK(!bidder_worklet_handle_);

  reporter_worklet_state_ = ReporterState::kBuyerReportWin;

  const blink::InterestGroup& interest_group =
      winning_bid_info_.storage_interest_group->interest_group;

  const SellerWinningBidInfo& bidder_auction = GetBidderAuction();
  std::optional<uint16_t> experiment_group_id =
      InterestGroupAuction::GetBuyerExperimentId(*bidder_auction.auction_config,
                                                 interest_group.owner);
  // While this has no effect when calling reportWin(), it's best to set it to
  // the same value to maximize the chance of finding a worklet to reuse.
  std::string trusted_bidder_signals_slot_size_param =
      InterestGroupAuction::CreateTrustedBiddingSignalsSlotSizeParam(
          *bidder_auction.auction_config,
          interest_group.trusted_bidding_signals_slot_size_mode);

  // base::Unretained is safe to use for these callbacks because destroying
  // `bidder_worklet_handle_` will prevent the callbacks from being invoked, if
  // `this` is destroyed while still waiting on the callbacks.
  auction_worklet_manager_->RequestBidderWorklet(
      devtools_auction_id_, interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url,
      /*needs_cors_for_additional_bid=*/
      winning_bid_info_.provided_as_additional_bid, experiment_group_id,
      trusted_bidder_signals_slot_size_param,
      interest_group.trusted_bidding_signals_coordinator,
      base::BindOnce(&InterestGroupAuctionReporter::OnBidderWorkletReceived,
                     base::Unretained(this), signals_for_winner),
      base::BindOnce(&InterestGroupAuctionReporter::OnBidderWorkletFatalError,
                     base::Unretained(this)),
      bidder_worklet_handle_, /*auction_metrics_recorder=*/nullptr);
}

void InterestGroupAuctionReporter::OnBidderWorkletReceived(
    const std::string& signals_for_winner) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "fledge", "bidder_worklet_report_win",
      top_level_seller_winning_bid_info_.trace_id);

  const SellerWinningBidInfo& seller_info = GetBidderAuction();
  const blink::AuctionConfig* auction_config = seller_info.auction_config;
  std::optional<std::string> per_buyer_signals =
      InterestGroupAuction::GetPerBuyerSignals(
          *auction_config,
          winning_bid_info_.storage_interest_group->interest_group.owner);

  auction_worklet::mojom::ReportingIdField reporting_id_field =
      auction_worklet::mojom::ReportingIdField::kNone;
  std::optional<std::string> interest_group_name_reporting_id;
  std::optional<std::string> buyer_reporting_id;
  std::optional<std::string> buyer_and_seller_reporting_id;
  std::optional<std::string> selected_buyer_and_seller_reporting_id;

  const blink::InterestGroup::Ad& chosen_ad = ChosenAd(
      winning_bid_info_.storage_interest_group, winning_bid_info_.render_url);

  // If k-anonymity enforcement is on we can only reveal the winning reporting
  // id in reportWin if the winning ad's reporting_ads_kanon entry is
  // k-anonymous.
  //
  // An exception to this is contextual bids, which have access to page
  // information anyway.
  if (winning_bid_info_.provided_as_additional_bid ||
      IsKAnonForReporting(
          winning_bid_info_.storage_interest_group, chosen_ad,
          winning_bid_info_.selected_buyer_and_seller_reporting_id)) {
    SetReportWinReportingIds(
        winning_bid_info_.storage_interest_group->interest_group.name,
        winning_bid_info_.selected_buyer_and_seller_reporting_id, chosen_ad,
        reporting_id_field, interest_group_name_reporting_id,
        buyer_reporting_id, buyer_and_seller_reporting_id,
        selected_buyer_and_seller_reporting_id);
  }
  base::UmaHistogramEnumeration(
      top_level_seller_winning_bid_info_.saved_response.has_value()
          ? "Ads.InterestGroup.ServerAuction.ReportingIdType"
          : "Ads.InterestGroup.Auction.ReportingIdType",
      reporting_id_field);

  bidder_worklet_handle_->AuthorizeSubresourceUrls(
      *seller_info.subresource_url_builder);

  std::optional<double> rounded_ad_cost;
  if (winning_bid_info_.ad_cost.has_value()) {
    rounded_ad_cost = RoundStochasticallyToKBits(
        winning_bid_info_.ad_cost.value(), kFledgeAdCostReportingBits.Get());
  }
  std::optional<uint16_t> noised_and_masked_modeling_signals;
  if (winning_bid_info_.modeling_signals) {
    noised_and_masked_modeling_signals =
        NoiseAndMaskModelingSignals(*winning_bid_info_.modeling_signals);
  }

  double highest_scoring_other_bid;
  std::optional<blink::AdCurrency> highest_scoring_other_bid_currency;
  bool made_highest_scoring_other_bid;
  InterestGroupAuction::PostAuctionSignals::
      FillRelevantHighestScoringOtherBidInfo(
          winning_bid_info_.storage_interest_group->interest_group.owner,
          seller_info.highest_scoring_other_bid_owner,
          seller_info.highest_scoring_other_bid,
          seller_info.highest_scoring_other_bid_in_seller_currency,
          auction_config->non_shared_params.seller_currency,
          made_highest_scoring_other_bid, highest_scoring_other_bid,
          highest_scoring_other_bid_currency);

  // While reportWin() itself always gets the bid back in its own currency,
  // for private aggregation we want to do things in seller currency if
  // it's enabled.
  double winning_bid_for_aggregation =
      auction_config->non_shared_params.seller_currency
          ? seller_info.bid_in_seller_currency
          : winning_bid_info_.bid;

  bidder_worklet_handle_->GetBidderWorklet()->ReportWin(
      winning_bid_info_.provided_as_additional_bid,
      interest_group_name_reporting_id, buyer_reporting_id,
      buyer_and_seller_reporting_id, selected_buyer_and_seller_reporting_id,
      auction_config->non_shared_params.auction_signals.value(),
      per_buyer_signals,
      InterestGroupAuction::GetDirectFromSellerPerBuyerSignals(
          seller_info.subresource_url_builder.get(),
          winning_bid_info_.storage_interest_group->interest_group.owner),
      InterestGroupAuction::GetDirectFromSellerPerBuyerSignalsHeaderAdSlot(
          *seller_info.direct_from_seller_signals_header_ad_slot,
          winning_bid_info_.storage_interest_group->interest_group.owner),
      InterestGroupAuction::GetDirectFromSellerAuctionSignals(
          seller_info.subresource_url_builder.get()),
      InterestGroupAuction::GetDirectFromSellerAuctionSignalsHeaderAdSlot(
          *seller_info.direct_from_seller_signals_header_ad_slot),
      signals_for_winner, kanon_mode_, bid_is_kanon_,
      winning_bid_info_.render_url, seller_info.rounded_bid,
      winning_bid_info_.bid_currency,
      /*browser_signal_highest_scoring_other_bid=*/
      RoundStochasticallyToKBits(highest_scoring_other_bid,
                                 kFledgeBidReportingBits.Get()),
      highest_scoring_other_bid_currency, made_highest_scoring_other_bid,
      rounded_ad_cost, noised_and_masked_modeling_signals,
      NoiseAndBucketJoinCount(
          // Browser signals may be null in tests.
          winning_bid_info_.storage_interest_group->bidding_browser_signals
              ? winning_bid_info_.storage_interest_group
                    ->bidding_browser_signals->join_count
              : 1),
      NoiseAndBucketRecency(
          base::Time::Now() -
          winning_bid_info_.storage_interest_group->join_time),
      auction_config->seller,
      /*browser_signal_top_level_seller_origin=*/
      component_seller_winning_bid_info_
          ? top_level_seller_winning_bid_info_.auction_config->seller
          : std::optional<url::Origin>(),
      auction_config->non_shared_params.reporting_timeout,
      winning_bid_info_.bidding_signals_data_version,
      top_level_seller_winning_bid_info_.trace_id,
      base::BindOnce(&InterestGroupAuctionReporter::OnBidderReportWinComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     winning_bid_for_aggregation, highest_scoring_other_bid));
}

void InterestGroupAuctionReporter::OnBidderWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  // Nothing more to do. Act as if the worklet completed as normal, with no
  // results.
  OnBidderReportWinComplete(
      /*winning_bid=*/0.0,
      /*highest_scoring_other_bid=*/0.0,
      /*bidder_report_url=*/std::nullopt,
      /*bidder_ad_beacon_map=*/{},
      /*bidder_ad_macro_map=*/{},
      /*pa_requests=*/{},
      /*timing_metrics=*/auction_worklet::mojom::BidderTimingMetrics::New(),
      errors);
}

void InterestGroupAuctionReporter::OnBidderReportWinComplete(
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<GURL>& bidder_report_url,
    const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
    const base::flat_map<std::string, std::string>& bidder_ad_macro_map,
    PrivateAggregationRequests pa_requests,
    auction_worklet::mojom::BidderTimingMetricsPtr timing_metrics,
    const std::vector<std::string>& errors) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "bidder_worklet_report_win",
                                  top_level_seller_winning_bid_info_.trace_id);

  reporter_worklet_state_ = ReporterState::kAllWorkletsCompleted;

  bidder_worklet_handle_.reset();

  log_private_aggregation_requests_callback_.Run(pa_requests);

  PrivateAggregationKey agg_key = {
      winning_bid_info_.storage_interest_group->interest_group.owner,
      winning_bid_info_.storage_interest_group->interest_group
          .aggregation_coordinator_origin};
  PrivateAggregationTimings timings;

  timings.script_run_time = timing_metrics->script_latency;

  PrivateAggregationParticipantData& participant_data =
      all_participants_data_[static_cast<size_t>(
          PrivateAggregationPhase::kBidder)];
  AuctionMetricsRecorder::LatencyAggregator code_fetch_time;
  if (timing_metrics->js_fetch_latency.has_value()) {
    code_fetch_time.RecordLatency(*timing_metrics->js_fetch_latency);
  }
  if (timing_metrics->wasm_fetch_latency.has_value()) {
    code_fetch_time.RecordLatency(*timing_metrics->wasm_fetch_latency);
  }
  participant_data.average_code_fetch_time =
      code_fetch_time.GetNumRecords() != 0 ? code_fetch_time.GetMeanLatency()
                                           : base::TimeDelta();
  participant_data.percent_scripts_timeout =
      timing_metrics->script_timed_out ? 100 : 0;

  if (!ValidateReportingPrivateAggregationRequests(pa_requests)) {
    pa_requests.clear();
  }
  for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
       pa_requests) {
    // Only winner's reportWin() gets executed, so is_winner is true, which
    // results in "reserved.loss" reports not being reported. Bid reject reason
    // is not meaningful thus not supported in reportWin(), so it is set to
    // std::nullopt.
    std::optional<PrivateAggregationRequestWithEventType> converted_request =
        FillInPrivateAggregationRequest(
            std::move(request), winning_bid,
            /*highest_scoring_other_bid=*/highest_scoring_other_bid,
            /*reject_reason=*/std::nullopt, participant_data, timings,
            /*is_winner=*/true);

    if (converted_request.has_value()) {
      PrivateAggregationRequestWithEventType converted_request_value =
          std::move(converted_request.value());
      const std::optional<std::string>& event_type =
          converted_request_value.event_type;
      if (event_type.has_value()) {
        // The request has a non-reserved event type.
        private_aggregation_requests_non_reserved_[event_type.value()]
            .emplace_back(std::move(converted_request_value.request));
      } else {
        private_aggregation_requests_reserved_[agg_key].emplace_back(
            std::move(converted_request_value.request));
      }
    }
  }

  std::vector<std::string> validation_errors;
  if (!AddReportWinResult(
          winning_bid_info_.storage_interest_group->interest_group.owner,
          bidder_report_url, bidder_ad_beacon_map, bidder_ad_macro_map,
          validation_errors)) {
    for (const auto& error : validation_errors) {
      mojo::ReportBadMessage(error);
    }
  }

  // If any event-level reports were queued, send them now, if the winning ad
  // has been navigated to.
  SendPendingReportsIfNavigated();

  OnReportingComplete(errors);
}

bool InterestGroupAuctionReporter::AddReportWinResult(
    const url::Origin& bidder_origin,
    const std::optional<GURL>& bidder_report_url,
    const base::flat_map<std::string, GURL>& bidder_ad_beacon_map,
    const std::optional<base::flat_map<std::string, std::string>>&
        bidder_ad_macro_map,
    std::vector<std::string>& errors_out) {
  // This will be cleared if any beacons are invalid.
  base::flat_map<std::string, GURL> validated_bidder_ad_beacon_map =
      bidder_ad_beacon_map;
  for (const auto& element : bidder_ad_beacon_map) {
    if (!IsEventLevelReportingUrlValid(element.second)) {
      errors_out.push_back(base::StrCat(
          {"Invalid bidder beacon URL for '", element.first, "'"}));
      // Drop the entire beacon map if part of it is invalid. No need to treat
      // the rest of the data received from the worklet as invalid - all fields
      // are validated and consumed independently, and it's not worth the
      // complexity to make sure everything is dropped when a field is invalid.
      validated_bidder_ad_beacon_map.clear();
      break;
    }
  }
  // Convert `bidder_ad_macro_map` to a vector of (${macro_name}, macro_value)
  // pairs as fenced frame reporter expects.
  std::optional<std::vector<std::pair<std::string, std::string>>>
      bidder_macros = std::nullopt;
  if (bidder_ad_macro_map.has_value()) {
    bidder_macros.emplace();
    for (const auto& [macro_name, macro_value] : bidder_ad_macro_map.value()) {
      bidder_macros->emplace_back("${" + macro_name + "}", macro_value);
    }
  }
  fenced_frame_reporter_->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer, bidder_origin,
      std::move(validated_bidder_ad_beacon_map), std::move(bidder_macros));

  if (bidder_report_url) {
    if (!IsEventLevelReportingUrlValid(*bidder_report_url)) {
      errors_out.push_back("Invalid bidder report URL");
      // No need to skip rest of work on failure - all fields are validated and
      // consumed independently, and it's not worth the complexity to make sure
      // everything is dropped when a field is invalid.
    } else {
      AddPendingReportUrl(*bidder_report_url);
    }
  }
  return errors_out.empty();
}

void InterestGroupAuctionReporter::OnReportingComplete(
    const std::vector<std::string>& errors) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "reporting_phase",
                                  top_level_seller_winning_bid_info_.trace_id);
  TRACE_EVENT_NESTABLE_ASYNC_END0("fledge", "auction",
                                  top_level_seller_winning_bid_info_.trace_id);
  errors_.insert(errors_.end(), errors.begin(), errors.end());
  reporting_complete_ = true;
  MaybeSendPrivateAggregationReports();
  MaybeInvokeCallback();
}

void InterestGroupAuctionReporter::OnNavigateToWinningAd(
    FrameTreeNodeId frame_tree_node_id) {
  if (navigated_to_winning_ad_) {
    return;
  }
  navigated_to_winning_ad_ = true;

  base::UmaHistogramTimes(
      base::StrCat(
          {"Ads.InterestGroup.",
           top_level_seller_winning_bid_info_.saved_response.has_value()
               ? "ServerAuction"
               : "Auction",
           ".ReportDelay"}),
      base::TimeTicks::Now() - start_time_);

  // Send any pending reports that are gathered as reports run.
  SendPendingReportsIfNavigated();
  MaybeSendPrivateAggregationReports();

  // Send pre-populated real time reports. Note that `real_time_contributions_`
  // will be converted to a histogram in EnqueueRealTimeReports().
  interest_group_manager_->EnqueueRealTimeReports(
      std::move(real_time_contributions_), ad_auction_page_data_callback_,
      frame_tree_node_id, frame_origin_, *client_security_state_,
      url_loader_factory_);
  real_time_contributions_.clear();

  // Send pre-populated reports. Send these after the main reports, since
  // reports are sent over the network in FIFO order.
  interest_group_manager_->EnqueueReports(
      InterestGroupManagerImpl::ReportType::kDebugWin,
      std::move(debug_win_report_urls_), frame_tree_node_id, frame_origin_,
      *client_security_state_, url_loader_factory_);
  debug_win_report_urls_.clear();
  interest_group_manager_->EnqueueReports(
      InterestGroupManagerImpl::ReportType::kDebugLoss,
      std::move(debug_loss_report_urls_), frame_tree_node_id, frame_origin_,
      *client_security_state_, url_loader_factory_);
  debug_loss_report_urls_.clear();

  interest_group_manager_->RecordInterestGroupBids(interest_groups_that_bid_);
  interest_groups_that_bid_.clear();

  const blink::InterestGroup& winning_group =
      winning_bid_info_.storage_interest_group->interest_group;
  InterestGroupManagerImpl::InterestGroupObserver::AccessType win_type =
      InterestGroupManagerImpl::InterestGroupObserver::kWin;
  if (winning_bid_info_.provided_as_additional_bid) {
    win_type =
        InterestGroupManagerImpl::InterestGroupObserver::kAdditionalBidWin;
  } else {
    interest_group_manager_->RecordInterestGroupWin(
        blink::InterestGroupKey(winning_group.owner, winning_group.name),
        winning_bid_info_.ad_metadata);
  }
  interest_group_manager_->NotifyInterestGroupAccessed(
      devtools_auction_id_, win_type, winning_group.owner, winning_group.name,
      component_seller_winning_bid_info_.has_value()
          ? base::optional_ref<const url::Origin>(
                component_seller_winning_bid_info_->auction_config->seller)
          : base::optional_ref<const url::Origin>(),
      /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt);

  interest_group_manager_->RegisterAdKeysAsJoined(
      std::move(k_anon_keys_to_join_));

  MaybeInvokeCallback();
}

void InterestGroupAuctionReporter::MaybeInvokeCallback() {
  DCHECK(callback_);
  if (reporting_complete_ && navigated_to_winning_ad_) {
    // All report URL should have been passed to the InterestGroupManager.
    DCHECK(pending_report_urls_.empty());

    std::move(callback_).Run();
  }
}

const InterestGroupAuctionReporter::SellerWinningBidInfo&
InterestGroupAuctionReporter::GetBidderAuction() {
  if (component_seller_winning_bid_info_) {
    return component_seller_winning_bid_info_.value();
  }
  return top_level_seller_winning_bid_info_;
}

void InterestGroupAuctionReporter::AddPendingReportUrl(const GURL& report_url) {
  if (!CheckReportUrl(report_url)) {
    return;
  }
  pending_report_urls_.push_back(report_url);
}

void InterestGroupAuctionReporter::SendPendingReportsIfNavigated() {
  if (!navigated_to_winning_ad_) {
    return;
  }
  FrameTreeNodeId frame_tree_node_id =
      auction_worklet_manager_->GetFrameTreeNodeID();
  interest_group_manager_->EnqueueReports(
      InterestGroupManagerImpl::ReportType::kSendReportTo,
      std::move(pending_report_urls_), frame_tree_node_id, frame_origin_,
      *client_security_state_, url_loader_factory_);
  pending_report_urls_.clear();
}

void InterestGroupAuctionReporter::MaybeSendPrivateAggregationReports() {
  if (!navigated_to_winning_ad_ || !reporting_complete_) {
    return;
  }
  OnFledgePrivateAggregationRequests(
      private_aggregation_manager_, main_frame_origin_,
      std::move(private_aggregation_requests_reserved_));
  private_aggregation_requests_reserved_.clear();

  if (base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) &&
      blink::features::kPrivateAggregationApiEnabledInProtectedAudience.Get() &&
      blink::features::kPrivateAggregationApiProtectedAudienceExtensionsEnabled
          .Get()) {
    fenced_frame_reporter_->OnForEventPrivateAggregationRequestsReceived(
        std::move(private_aggregation_requests_non_reserved_));
  }
  // TODO(qingxinwu): Check the feature flags when collecting PA requests in
  // browser process, and report a bad message if PA requests are received when
  // the feature flags are disabled. Then CHECK that
  // `private_aggregation_requests_non_reserved_` is empty here.
  private_aggregation_requests_non_reserved_.clear();
}

bool InterestGroupAuctionReporter::CheckReportUrl(const GURL& url) {
  if (!GetContentClient()
           ->browser()
           ->IsPrivacySandboxReportingDestinationAttested(
               browser_context_, url::Origin::Create(url),
               PrivacySandboxInvokingAPI::kProtectedAudience)) {
    errors_.push_back(base::StringPrintf(
        "The reporting destination %s is not attested for Protected Audience.",
        url.spec().c_str()));
    return false;
  }

  return true;
}

void InterestGroupAuctionReporter::EnforceAttestationsReportUrls(
    std::vector<GURL>& urls) {
  std::erase_if(urls, [this](const GURL& url) { return !CheckReportUrl(url); });
}

}  // namespace content
