// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// InterestGroupUpdate represents the results of parsing a JSON update for a
// stored blink::InterestGroup file. It contains all updatable fields of a
// InterestGroup - that is, everything but `name`, `origin`, `expiry`, and
// `user_bidding_signals`. All fields are optional, even ones that are mandatory
// in an InterestGroup, since the value of the original InterestGroup will be
// used when they're not present in an InterestGroupUpdate.
struct CONTENT_EXPORT InterestGroupUpdate {
  InterestGroupUpdate();
  InterestGroupUpdate(const InterestGroupUpdate&);
  InterestGroupUpdate(InterestGroupUpdate&&);
  ~InterestGroupUpdate();

  absl::optional<double> priority;
  absl::optional<bool> enable_bidding_signals_prioritization;
  absl::optional<base::flat_map<std::string, double>> priority_vector;
  // Unlike other fields, this is merged with the previous value, so can keep
  // old overrides around. Keys mapped to nullopt are deleted.
  absl::optional<base::flat_map<std::string, absl::optional<double>>>
      priority_signals_overrides;
  absl::optional<base::flat_map<url::Origin, blink::SellerCapabilitiesType>>
      seller_capabilities;
  absl::optional<blink::SellerCapabilitiesType> all_sellers_capabilities;
  absl::optional<blink::InterestGroup::ExecutionMode> execution_mode;
  absl::optional<GURL> bidding_url;
  absl::optional<GURL> bidding_wasm_helper_url;
  absl::optional<GURL> daily_update_url;
  absl::optional<GURL> trusted_bidding_signals_url;
  absl::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  absl::optional<std::vector<blink::InterestGroup::Ad>> ads, ad_components;
  absl::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes;
  absl::optional<base::flat_map<std::string, std::vector<std::string>>>
      size_groups;
  absl::optional<blink::AuctionServerRequestFlags> auction_server_request_flags;
  absl::optional<blink::InterestGroup::AdditionalBidKey> additional_bid_key;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
