// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_response.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {
const size_t kFramingHeaderSize = 5;  // bytes
const uint8_t kExpectedHeaderVersionInfo = 0x02;

// TODO(crbug.com/40215445): Replace with `base/numerics/byte_conversions.h` if
// available.
absl::uint128 U128FromBigEndian(std::vector<uint8_t> bytes) {
  absl::uint128 result = 0;
  for (unsigned char byte : bytes) {
    result = (result << 8) | byte;
  }
  return result;
}

}  // namespace

std::optional<base::span<const uint8_t>>
ExtractCompressedBiddingAndAuctionResponse(
    base::span<const uint8_t> decrypted_data) {
  if (decrypted_data.size() < kFramingHeaderSize) {
    // Response is too short
    return std::nullopt;
  }
  if (decrypted_data[0] != kExpectedHeaderVersionInfo) {
    // Bad version and compression information
    return std::nullopt;
  }
  size_t response_length = (decrypted_data[1] << 24) |
                           (decrypted_data[2] << 16) |
                           (decrypted_data[3] << 8) | (decrypted_data[4] << 0);
  if (decrypted_data.size() < kFramingHeaderSize + response_length) {
    // Incomplete Data.
    return std::nullopt;
  }
  return decrypted_data.subspan(kFramingHeaderSize, response_length);
}

BiddingAndAuctionResponse::BiddingAndAuctionResponse(
    BiddingAndAuctionResponse&&) = default;
BiddingAndAuctionResponse& BiddingAndAuctionResponse::operator=(
    BiddingAndAuctionResponse&&) = default;

BiddingAndAuctionResponse::BiddingAndAuctionResponse() = default;
BiddingAndAuctionResponse::~BiddingAndAuctionResponse() = default;

// static
std::optional<BiddingAndAuctionResponse> BiddingAndAuctionResponse::TryParse(
    base::Value input,
    const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
    const base::flat_map<blink::InterestGroupKey, url::Origin>&
        group_pagg_coordinators) {
  BiddingAndAuctionResponse output;
  base::Value::Dict* input_dict = input.GetIfDict();
  if (!input_dict) {
    return std::nullopt;
  }

  base::Value::Dict* error_struct = input_dict->FindDict("error");
  if (error_struct) {
    std::string* message = error_struct->FindString("message");
    if (message) {
      output.error = *message;
    } else {
      output.error = "Unknown server error";
    }
    output.is_chaff = true;  // Mark it as a no-bid result.
    return std::move(output);
  }

  std::optional<bool> maybe_is_chaff = input_dict->FindBool("isChaff");
  if (maybe_is_chaff && maybe_is_chaff.value()) {
    output.is_chaff = true;
    return std::move(output);
  }
  output.is_chaff = false;

  std::string* maybe_render_url = input_dict->FindString("adRenderURL");
  if (!maybe_render_url) {
    return std::nullopt;
  }
  output.ad_render_url = GURL(*maybe_render_url);
  if (!output.ad_render_url.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(output.ad_render_url)) {
    return std::nullopt;
  }
  base::Value* components_value = input_dict->Find("components");
  if (components_value) {
    base::Value::List* components = components_value->GetIfList();
    if (!components) {
      return std::nullopt;
    }
    for (const base::Value& component_val : *components) {
      const std::string* component_str = component_val.GetIfString();
      if (!component_str) {
        return std::nullopt;
      }
      GURL component(*component_str);
      if (!component.is_valid() ||
          !network::IsUrlPotentiallyTrustworthy(component)) {
        return std::nullopt;
      }
      output.ad_components.emplace_back(std::move(component));
    }
  }
  std::string* maybe_name = input_dict->FindString("interestGroupName");
  if (!maybe_name) {
    return std::nullopt;
  }
  output.interest_group_name = *maybe_name;

  std::string* maybe_owner = input_dict->FindString("interestGroupOwner");
  if (!maybe_owner) {
    return std::nullopt;
  }
  output.interest_group_owner = url::Origin::Create(GURL(*maybe_owner));
  if (!network::IsOriginPotentiallyTrustworthy(output.interest_group_owner)) {
    return std::nullopt;
  }

  base::Value::Dict* bidding_groups = input_dict->FindDict("biddingGroups");
  if (!bidding_groups) {
    return std::nullopt;
  }
  for (const auto owner_groups : *bidding_groups) {
    url::Origin owner = url::Origin::Create(GURL(owner_groups.first));
    if (!network::IsOriginPotentiallyTrustworthy(owner)) {
      return std::nullopt;
    }

    auto it = group_names.find(owner);
    if (it == group_names.end()) {
      return std::nullopt;
    }
    const std::vector<std::string>& names = it->second;

    const base::Value::List* groups = owner_groups.second.GetIfList();
    if (!groups) {
      return std::nullopt;
    }

    for (const auto& group : *groups) {
      std::optional<int> maybe_group_idx = group.GetIfInt();
      if (!maybe_group_idx) {
        return std::nullopt;
      }
      if (*maybe_group_idx < 0 ||
          static_cast<size_t>(*maybe_group_idx) >= names.size()) {
        return std::nullopt;
      }
      output.bidding_groups.emplace_back(owner, names[*maybe_group_idx]);
    }
  }

  output.score = input_dict->FindDouble("score");
  output.bid = input_dict->FindDouble("bid");

  std::string* maybe_currency = input_dict->FindString("bidCurrency");
  if (maybe_currency) {
    if (!blink::IsValidAdCurrencyCode(*maybe_currency)) {
      return std::nullopt;
    }
    output.bid_currency = blink::AdCurrency::From(*maybe_currency);
  }

  base::Value::Dict* win_reporting_urls =
      input_dict->FindDict("winReportingURLs");
  if (win_reporting_urls) {
    base::Value::Dict* buyer_reporting =
        win_reporting_urls->FindDict("buyerReportingURLs");
    if (buyer_reporting) {
      output.buyer_reporting = ReportingURLs::TryParse(buyer_reporting);
    }
    base::Value::Dict* top_level_seller_reporting =
        win_reporting_urls->FindDict("topLevelSellerReportingURLs");
    if (top_level_seller_reporting) {
      output.top_level_seller_reporting =
          ReportingURLs::TryParse(top_level_seller_reporting);
    }
    base::Value::Dict* component_seller_reporting =
        win_reporting_urls->FindDict("componentSellerReportingURLs");
    if (component_seller_reporting) {
      output.component_seller_reporting =
          ReportingURLs::TryParse(component_seller_reporting);
    }
  }
  std::string* maybe_top_level_seller =
      input_dict->FindString("topLevelSeller");
  if (maybe_top_level_seller) {
    url::Origin top_level_seller =
        url::Origin::Create(GURL(*maybe_top_level_seller));
    if (!network::IsOriginPotentiallyTrustworthy(top_level_seller)) {
      return std::nullopt;
    }
    output.top_level_seller = std::move(top_level_seller);
  }
  std::string* maybe_ad_metadata = input_dict->FindString("adMetadata");
  if (maybe_ad_metadata) {
    output.ad_metadata = *maybe_ad_metadata;
  }
  std::string* maybe_buyer_reporting_id =
      input_dict->FindString("buyerReportingId");
  if (maybe_buyer_reporting_id) {
    output.buyer_reporting_id = *maybe_buyer_reporting_id;
  }
  std::string* maybe_buyer_and_seller_reporting_id =
      input_dict->FindString("buyerAndSellerReportingId");
  if (maybe_buyer_and_seller_reporting_id) {
    output.buyer_and_seller_reporting_id = *maybe_buyer_and_seller_reporting_id;
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrivateAggregationApi) &&
      blink::features::kPrivateAggregationApiEnabledInProtectedAudience.Get() &&
      base::FeatureList::IsEnabled(features::kEnableBandAPrivateAggregation)) {
    const base::Value::List* pagg_response =
        input_dict->FindList("paggResponse");
    if (pagg_response) {
      TryParsePAggResponse(*pagg_response, group_names, group_pagg_coordinators,
                           output);
    }
  }

  if (base::FeatureList::IsEnabled(features::kEnableBandASampleDebugReports)) {
    base::Value::List* for_debugging_only_reports =
        input_dict->FindList("debugReports");
    if (for_debugging_only_reports) {
      TryParseForDebuggingOnlyReports(*for_debugging_only_reports, output);
    }
  }

  base::Value::Dict* triggered_updates = input_dict->FindDict("updateGroups");
  if (triggered_updates) {
    for (const auto owner_groups : *triggered_updates) {
      url::Origin owner = url::Origin::Create(GURL(owner_groups.first));
      auto it = group_names.find(owner);
      if (it == group_names.end()) {
        continue;
      }
      const std::vector<std::string>& names = it->second;

      const base::Value::List* groups = owner_groups.second.GetIfList();
      if (!groups) {
        continue;
      }
      for (const auto& group : *groups) {
        const base::Value::Dict* group_dict = group.GetIfDict();
        if (!group_dict) {
          continue;
        }
        std::optional<int> maybe_group_idx = group_dict->FindInt("index");
        if (!maybe_group_idx) {
          continue;
        }
        if (*maybe_group_idx < 0 ||
            static_cast<size_t>(*maybe_group_idx) >= names.size()) {
          continue;
        }

        std::optional<int> maybe_update_if_older_than =
            group_dict->FindInt("updateIfOlderThanMs");
        if (!maybe_update_if_older_than) {
          continue;
        }
        output.triggered_updates[blink::InterestGroupKey(
            owner, names[*maybe_group_idx])] =
            base::Milliseconds(*maybe_update_if_older_than);
      }
    }
  }

  output.result = AuctionResult::kSuccess;
  return std::move(output);
}

// static
void BiddingAndAuctionResponse::TryParsePAggResponse(
    const base::Value::List& pagg_response,
    const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
    const base::flat_map<blink::InterestGroupKey, url::Origin>&
        group_pagg_coordinators,
    BiddingAndAuctionResponse& output) {
  for (const auto& per_origin_response : pagg_response) {
    const base::Value::Dict* per_origin_response_dict =
        per_origin_response.GetIfDict();
    if (!per_origin_response_dict) {
      continue;
    }

    const std::string* maybe_reporting_origin =
        per_origin_response_dict->FindString("reportingOrigin");
    if (!maybe_reporting_origin) {
      continue;
    }
    url::Origin reporting_origin =
        url::Origin::Create(GURL(*maybe_reporting_origin));
    if (!network::IsOriginPotentiallyTrustworthy(reporting_origin)) {
      continue;
    }

    const base::Value::List* ig_contributions =
        per_origin_response_dict->FindList("igContributions");
    if (ig_contributions) {
      TryParsePAggIgContributions(*ig_contributions, reporting_origin,
                                  group_pagg_coordinators, group_names, output);
    }
  }
}

// static
void BiddingAndAuctionResponse::TryParsePAggIgContributions(
    const base::Value::List& ig_contributions,
    const url::Origin& reporting_origin,
    const base::flat_map<blink::InterestGroupKey, url::Origin>&
        group_pagg_coordinators,
    const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
    BiddingAndAuctionResponse& output) {
  auto single_origin_group_names_it = group_names.find(reporting_origin);
  for (const auto& ig_contribution : ig_contributions) {
    const base::Value::Dict* ig_contribution_dict = ig_contribution.GetIfDict();
    if (!ig_contribution_dict) {
      continue;
    }
    std::optional<int> maybe_ig_index =
        ig_contribution_dict->FindInt("igIndex");
    const std::string* maybe_coordinator =
        ig_contribution_dict->FindString("coordinator");
    std::optional<url::Origin> aggregation_coordinator_origin;
    if (maybe_coordinator) {
      aggregation_coordinator_origin =
          url::Origin::Create(GURL(*maybe_coordinator));
      if (!network::IsOriginPotentiallyTrustworthy(
              *aggregation_coordinator_origin)) {
        continue;
      }
    } else if (maybe_ig_index.has_value()) {
      if (single_origin_group_names_it == group_names.end()) {
        continue;
      }
      const std::vector<std::string>& names =
          single_origin_group_names_it->second;
      if (*maybe_ig_index < 0 ||
          static_cast<size_t>(*maybe_ig_index) >= names.size()) {
        continue;
      }
      auto it = group_pagg_coordinators.find(
          blink::InterestGroupKey(reporting_origin, names[*maybe_ig_index]));
      if (it != group_pagg_coordinators.end()) {
        aggregation_coordinator_origin = it->second;
      }
    }
    std::optional<bool> maybe_component_win =
        ig_contribution_dict->FindBool("componentWin");
    const base::Value::List* event_contributions =
        ig_contribution_dict->FindList("eventContributions");
    if (event_contributions) {
      TryParsePAggEventContributions(
          *event_contributions, reporting_origin,
          aggregation_coordinator_origin,
          maybe_component_win.has_value() && *maybe_component_win, output);
    }
  }
}

// static
void BiddingAndAuctionResponse::TryParsePAggEventContributions(
    const base::Value::List& event_contributions,
    const url::Origin& reporting_origin,
    const std::optional<url::Origin>& aggregation_coordinator_origin,
    bool component_win,
    BiddingAndAuctionResponse& output) {
  // Used as key in `server_filtered_pagg_requests_reserved`.
  PrivateAggregationKey agg_key = {reporting_origin,
                                   aggregation_coordinator_origin};
  // Used as key in `component_win_pagg_requests`.
  PrivateAggregationPhaseKey agg_phase_key = {
      reporting_origin, PrivateAggregationPhase::kNonTopLevelSeller,
      aggregation_coordinator_origin};
  for (const auto& event_contribution : event_contributions) {
    const base::Value::Dict* event_contribution_dict =
        event_contribution.GetIfDict();
    if (!event_contribution_dict) {
      continue;
    }
    const std::string* event_type_str =
        event_contribution_dict->FindString("event");
    if (!event_type_str) {
      continue;
    }

    const base::Value::List* contributions =
        event_contribution_dict->FindList("contributions");
    if (contributions) {
      TryParsePAggContributions(*contributions, component_win, *event_type_str,
                                agg_phase_key, agg_key, output);
    }
  }
}

// static
void BiddingAndAuctionResponse::TryParsePAggContributions(
    const base::Value::List& contributions,
    bool component_win,
    const std::string& event_type_str,
    const PrivateAggregationPhaseKey& agg_phase_key,
    const PrivateAggregationKey& agg_key,
    BiddingAndAuctionResponse& output) {
  auction_worklet::mojom::EventTypePtr event_type =
      auction_worklet::ParsePrivateAggregationEventType(
          event_type_str,
          base::FeatureList::IsEnabled(
              blink::features::
                  kPrivateAggregationApiProtectedAudienceAdditionalExtensions));
  if (!event_type) {
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added
    // later.
    return;
  }
  for (const auto& contribution : contributions) {
    const base::Value::Dict* contribution_dict = contribution.GetIfDict();
    if (!contribution_dict) {
      continue;
    }
    const std::vector<uint8_t>* bucket = contribution_dict->FindBlob("bucket");
    std::optional<int> value = contribution_dict->FindInt("value");
    std::optional<uint64_t> filtering_id;
    if (base::FeatureList::IsEnabled(
            blink::features::kPrivateAggregationApiFilteringIds)) {
      filtering_id = contribution_dict->FindInt("filteringId");
    }
    if (!bucket || bucket->size() > 16 || !value.has_value() ||
        (filtering_id.has_value() && !IsValidFilteringId(filtering_id))) {
      continue;
    }
    if (component_win) {
      // Response contains all event types for a component winner, since it may
      // win or lose the top level auction. `request` needs to contain event
      // type because it's needed to decide whether it needs to be filtered out
      // based on the top level auction result.
      auction_worklet::mojom::PrivateAggregationRequestPtr request =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewForEventContribution(
                      auction_worklet::mojom::
                          AggregatableReportForEventContribution::New(
                              auction_worklet::mojom::ForEventSignalBucket::
                                  NewIdBucket(U128FromBigEndian(*bucket)),
                              auction_worklet::mojom::ForEventSignalValue::
                                  NewIntValue(*value),
                              filtering_id, event_type->Clone())),
              // TODO(qingxinwu): consider allowing this to be set
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
      output.component_win_pagg_requests[agg_phase_key].emplace_back(
          std::move(request));
    } else {
      // Server already filtered out not needed contributions based on final
      // auction result.
      auction_worklet::mojom::PrivateAggregationRequestPtr request =
          auction_worklet::mojom::PrivateAggregationRequest::New(
              auction_worklet::mojom::AggregatableReportContribution::
                  NewHistogramContribution(
                      blink::mojom::AggregatableReportHistogramContribution::
                          New(
                              /*bucket=*/U128FromBigEndian(*bucket),
                              /*value=*/*value,
                              /*filtering_id=*/filtering_id)),
              // TODO(qingxinwu): consider allowing this to be set
              blink::mojom::AggregationServiceMode::kDefault,
              blink::mojom::DebugModeDetails::New());
      if (event_type->is_reserved()) {
        output.server_filtered_pagg_requests_reserved[agg_key].emplace_back(
            std::move(request));
      } else {
        output.server_filtered_pagg_requests_non_reserved[event_type_str]
            .emplace_back(std::move(request));
      }
    }
  }
}

// static
void BiddingAndAuctionResponse::TryParseForDebuggingOnlyReports(
    const base::Value::List& for_debugging_only_reports,
    BiddingAndAuctionResponse& output) {
  for (const auto& per_origin_debug_reports : for_debugging_only_reports) {
    const base::Value::Dict* per_origin_debug_reports_dict =
        per_origin_debug_reports.GetIfDict();
    if (!per_origin_debug_reports_dict) {
      continue;
    }
    const std::string* maybe_ad_tech_origin =
        per_origin_debug_reports_dict->FindString("adTechOrigin");
    if (!maybe_ad_tech_origin) {
      continue;
    }
    url::Origin ad_tech_origin =
        url::Origin::Create(GURL(*maybe_ad_tech_origin));
    if (!network::IsOriginPotentiallyTrustworthy(ad_tech_origin)) {
      continue;
    }
    const base::Value::List* reports =
        per_origin_debug_reports_dict->FindList("reports");
    if (reports) {
      for (const auto& report : *reports) {
        const base::Value::Dict* report_dict = report.GetIfDict();
        if (!report_dict) {
          continue;
        }
        output.debugging_only_report_origins.emplace(ad_tech_origin);
        TryParseSingleDebugReport(ad_tech_origin, *report_dict, output);
      }
    }
  }
}

// static
void BiddingAndAuctionResponse::TryParseSingleDebugReport(
    const url::Origin& ad_tech_origin,
    const base::Value::Dict& report_dict,
    BiddingAndAuctionResponse& output) {
  std::optional<bool> maybe_component_win =
      report_dict.FindBool("componentWin");
  const std::string* maybe_url_str = report_dict.FindString("url");
  if (maybe_url_str) {
    GURL reporting_url(*maybe_url_str);
    if (!reporting_url.is_valid() ||
        !network::IsUrlPotentiallyTrustworthy(reporting_url)) {
      return;
    }
    if (!maybe_component_win.has_value() || !*maybe_component_win) {
      output.server_filtered_debugging_only_reports[ad_tech_origin]
          .emplace_back(reporting_url);
    } else {
      std::optional<bool> maybe_is_win_report =
          report_dict.FindBool("isWinReport");
      bool is_win_report =
          maybe_is_win_report.has_value() && *maybe_is_win_report;
      std::optional<bool> maybe_seller_report =
          report_dict.FindBool("isSellerReport");
      bool is_seller_report =
          maybe_seller_report.has_value() && *maybe_seller_report;
      output.component_win_debugging_only_reports[DebugReportKey(
          is_seller_report, is_win_report)] = reporting_url;
    }
  } else {
    // "url" field is allowed to be not set in debugReports, for cases like
    // forDebuggingOnly APIs were called but server side sampling filtered them
    // out. There's still an entry for this in debugReports to tell Chrome to
    // set cooldown for the ad tech origin.
    // Component auction winner's reports need to be filtered on client side, so
    // their urls will always be set if corresponding forDebuggingOnly API is
    // called. Insert an entry to corresponding maps for `ad_tech_origin`.
    if (!maybe_component_win.has_value() || !*maybe_component_win) {
      if (!output.server_filtered_debugging_only_reports.contains(
              ad_tech_origin)) {
        output.server_filtered_debugging_only_reports[ad_tech_origin] = {};
      }
    }
  }
}

BiddingAndAuctionResponse::ReportingURLs::ReportingURLs() = default;
BiddingAndAuctionResponse::ReportingURLs::~ReportingURLs() = default;

BiddingAndAuctionResponse::ReportingURLs::ReportingURLs(ReportingURLs&&) =
    default;
BiddingAndAuctionResponse::ReportingURLs&
BiddingAndAuctionResponse::ReportingURLs::operator=(ReportingURLs&&) = default;

// static
std::optional<BiddingAndAuctionResponse::ReportingURLs>
BiddingAndAuctionResponse::ReportingURLs::TryParse(
    base::Value::Dict* input_dict) {
  ReportingURLs output;
  std::string* maybe_reporting_url = input_dict->FindString("reportingURL");
  if (maybe_reporting_url) {
    GURL reporting_url(*maybe_reporting_url);
    if (reporting_url.is_valid() &&
        network::IsUrlPotentiallyTrustworthy(reporting_url)) {
      output.reporting_url = std::move(reporting_url);
    }
  }
  base::Value::Dict* interaction_reporting =
      input_dict->FindDict("interactionReportingURLs");
  if (interaction_reporting) {
    std::vector<std::pair<std::string, GURL>> beacon_urls;
    for (const auto interaction : *interaction_reporting) {
      std::string* maybe_url = interaction.second.GetIfString();
      if (!maybe_url) {
        continue;
      }
      GURL beacon_url(*maybe_url);
      if (!beacon_url.is_valid() ||
          !network::IsUrlPotentiallyTrustworthy(beacon_url)) {
        continue;
      }
      beacon_urls.emplace_back(interaction.first, std::move(beacon_url));
    }
    output.beacon_urls =
        base::flat_map<std::string, GURL>(std::move(beacon_urls));
  }
  return std::move(output);
}

}  // namespace content
