// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/auction_result.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using PrivateAggregationRequests =
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

std::optional<base::span<const uint8_t>> CONTENT_EXPORT
ExtractCompressedBiddingAndAuctionResponse(
    base::span<const uint8_t> decrypted_data);

struct CONTENT_EXPORT BiddingAndAuctionResponse {
  BiddingAndAuctionResponse();
  ~BiddingAndAuctionResponse();

  BiddingAndAuctionResponse(BiddingAndAuctionResponse&& other);
  BiddingAndAuctionResponse& operator=(BiddingAndAuctionResponse&& other);

  static std::optional<BiddingAndAuctionResponse> TryParse(
      base::Value input,
      const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
      const base::flat_map<blink::InterestGroupKey, url::Origin>&
          group_pagg_coordinators);

  static void TryParsePAggResponse(
      const base::Value::List& pagg_response,
      const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
      const base::flat_map<blink::InterestGroupKey, url::Origin>&
          group_pagg_coordinators,
      BiddingAndAuctionResponse& output);

  static void TryParsePAggIgContributions(
      const base::Value::List& ig_contributions,
      const url::Origin& reporting_origin,
      const base::flat_map<blink::InterestGroupKey, url::Origin>&
          group_pagg_coordinators,
      const base::flat_map<url::Origin, std::vector<std::string>>& group_names,
      BiddingAndAuctionResponse& output);

  static void TryParsePAggEventContributions(
      const base::Value::List& event_contributions,
      const url::Origin& reporting_origin,
      const std::optional<url::Origin>& aggregation_coordinator_origin,
      bool component_win,
      BiddingAndAuctionResponse& output);

  static void TryParsePAggContributions(
      const base::Value::List& contributions,
      bool component_win,
      const std::string& event,
      const PrivateAggregationPhaseKey& agg_phase_key,
      const PrivateAggregationKey& agg_key,
      BiddingAndAuctionResponse& output);

  static void TryParseForDebuggingOnlyReports(
      const base::Value::List& for_debugging_only_reporting,
      BiddingAndAuctionResponse& output);

  static void TryParseSingleDebugReport(const url::Origin& ad_tech_origin,
                                        const base::Value::Dict& report_dict,
                                        BiddingAndAuctionResponse& output);

  struct CONTENT_EXPORT ReportingURLs {
    ReportingURLs();
    ~ReportingURLs();

    ReportingURLs(ReportingURLs&& other);
    ReportingURLs& operator=(ReportingURLs&& other);

    static std::optional<ReportingURLs> TryParse(base::Value::Dict* input_dict);

    std::optional<GURL> reporting_url;
    base::flat_map<std::string, GURL> beacon_urls;
  };

  struct CONTENT_EXPORT DebugReportKey {
    bool is_seller_report;
    bool is_win_report;

    bool operator<(const DebugReportKey& other) const {
      if (is_seller_report != other.is_seller_report) {
        return is_seller_report < other.is_seller_report;
      } else {
        return is_win_report < other.is_win_report;
      }
    }
  };

  // This is not part of the message from the server, but is a convenient place
  // to store the outcome if we finish parsing the response before the component
  // auctions start the bidding phase.
  AuctionResult result = AuctionResult::kInvalidServerResponse;

  bool is_chaff = false;  // indicates this response should be ignored.
  // TODO(behamilton): Add support for creative dimensions to the response from
  // the Bidding and Auction server.
  GURL ad_render_url;
  std::vector<GURL> ad_components;
  std::string interest_group_name;
  url::Origin interest_group_owner;
  std::vector<blink::InterestGroupKey> bidding_groups;
  std::optional<double> score, bid;
  std::optional<blink::AdCurrency> bid_currency;
  std::optional<url::Origin> top_level_seller;
  std::optional<std::string> ad_metadata;
  std::optional<std::string> buyer_reporting_id;
  std::optional<std::string> buyer_and_seller_reporting_id;

  std::optional<std::string> error;
  // The Bidding and Auction server uses the top_level_seller_reporting field
  // for single-level auctions.
  std::optional<ReportingURLs> buyer_reporting, top_level_seller_reporting,
      component_seller_reporting;

  // Private aggregation requests from component winning buyer/seller. These
  // need to be further filtered based on the final auction result.
  std::map<PrivateAggregationPhaseKey, PrivateAggregationRequests>
      component_win_pagg_requests;

  // Private aggregation contributions that has been filtered by the server,
  // which can all be sent without further filtering on auction result. These
  // include component losing buyers/sellers PAgg contributions, or
  // contributions from single level auctions or server orchestrated multi-level
  // auctions.
  std::map<PrivateAggregationKey, PrivateAggregationRequests>
      server_filtered_pagg_requests_reserved;
  std::map<std::string, PrivateAggregationRequests>
      server_filtered_pagg_requests_non_reserved;

  // forDebuggingOnly reports from component winning buyer/seller. These need to
  // be further filtered based on the final auction result.
  std::map<DebugReportKey, std::optional<GURL>>
      component_win_debugging_only_reports;

  // forDebuggingOnly reports that have been filtered by the server.
  std::map<url::Origin, std::vector<GURL>>
      server_filtered_debugging_only_reports;

  // Ad tech origins that have forDebuggingOnly reports from server. This is
  // used to get these origin's cooldown status.
  base::flat_set<url::Origin> debugging_only_report_origins;

  // Interest group updates triggered by "update if older than" signals.
  std::map<blink::InterestGroupKey, base::TimeDelta> triggered_updates;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_
