// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_response.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

namespace {
const size_t kFramingHeaderSize = 5;  // bytes
const uint8_t kExpectedHeaderVersionInfo = 0x02;
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
    const base::flat_map<url::Origin, std::vector<std::string>>& group_names) {
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
    base::Value::Dict* seller_reporting =
        win_reporting_urls->FindDict("topLevelSellerReportingURLs");
    if (seller_reporting) {
      output.seller_reporting = ReportingURLs::TryParse(seller_reporting);
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

  output.result = AuctionResult::kSuccess;
  return std::move(output);
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
