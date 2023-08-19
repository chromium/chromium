// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

absl::optional<base::span<const uint8_t>> CONTENT_EXPORT
ExtractCompressedBiddingAndAuctionResponse(
    base::span<const uint8_t> decrypted_data);

struct CONTENT_EXPORT BiddingAndAuctionResponse {
  BiddingAndAuctionResponse();
  ~BiddingAndAuctionResponse();

  BiddingAndAuctionResponse(BiddingAndAuctionResponse&& other);
  BiddingAndAuctionResponse& operator=(BiddingAndAuctionResponse&& other);

  static absl::optional<BiddingAndAuctionResponse> TryParse(
      base::Value input,
      const base::flat_map<url::Origin, std::vector<std::string>>& group_names);

  struct CONTENT_EXPORT ReportingURLs {
    ReportingURLs();
    ~ReportingURLs();

    ReportingURLs(ReportingURLs&& other);
    ReportingURLs& operator=(ReportingURLs&& other);

    static absl::optional<ReportingURLs> TryParse(
        base::Value::Dict* input_dict);

    absl::optional<GURL> reporting_url;
    base::flat_map<std::string, GURL> beacon_urls;
  };

  bool is_chaff;  // indicates this response should be ignored.
  // TODO(behamilton): Add support for creative dimensions to the response from
  // the Bidding and Auction server.
  GURL ad_render_url;
  std::vector<GURL> ad_components;
  std::string interest_group_name;
  url::Origin interest_group_owner;
  std::vector<blink::InterestGroupKey> bidding_groups;
  absl::optional<double> score, bid;

  absl::optional<std::string> error;
  absl::optional<ReportingURLs> buyer_reporting, seller_reporting;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BIDDING_AND_AUCTION_RESPONSE_H_
