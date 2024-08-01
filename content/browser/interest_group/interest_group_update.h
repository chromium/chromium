// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_

#include <stdint.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// InterestGroupUpdate represents the results of parsing a JSON update for a
// stored blink::InterestGroup file. It contains all updatable fields of a
// InterestGroup - that is, everything but `name`, `origin`, and `expiry`.
// All fields are optional, even ones that are mandatory in an InterestGroup,
// since the value of the original InterestGroup will be
// used when they're not present in an InterestGroupUpdate.
struct CONTENT_EXPORT InterestGroupUpdate {
  InterestGroupUpdate();
  InterestGroupUpdate(const InterestGroupUpdate&);
  InterestGroupUpdate(InterestGroupUpdate&&);
  ~InterestGroupUpdate();

  std::optional<double> priority;
  std::optional<bool> enable_bidding_signals_prioritization;
  std::optional<base::flat_map<std::string, double>> priority_vector;
  // Unlike other fields, this is merged with the previous value, so can keep
  // old overrides around. Keys mapped to nullopt are deleted.
  std::optional<base::flat_map<std::string, std::optional<double>>>
      priority_signals_overrides;
  std::optional<base::flat_map<url::Origin, blink::SellerCapabilitiesType>>
      seller_capabilities;
  std::optional<blink::SellerCapabilitiesType> all_sellers_capabilities;
  std::optional<blink::InterestGroup::ExecutionMode> execution_mode;
  std::optional<GURL> bidding_url;
  std::optional<GURL> bidding_wasm_helper_url;
  std::optional<GURL> daily_update_url;
  std::optional<GURL> trusted_bidding_signals_url;
  std::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  std::optional<blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode>
      trusted_bidding_signals_slot_size_mode;
  std::optional<int32_t> max_trusted_bidding_signals_url_length;
  // The trusted_bidding_signals_coordinator field in the interest group
  // configuration is optional and indicates whether the interest group is using
  // Key-Value v1 or v2. In this update, it has the capability to be cleared
  // when the value is `null` in JSON, allowing for a downgrade from KVv2 to
  // KVv1.
  std::optional<std::optional<url::Origin>> trusted_bidding_signals_coordinator;
  std::optional<std::string> user_bidding_signals;
  std::optional<std::vector<blink::InterestGroup::Ad>> ads, ad_components;
  std::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes;
  std::optional<base::flat_map<std::string, std::vector<std::string>>>
      size_groups;
  std::optional<blink::AuctionServerRequestFlags> auction_server_request_flags;
  std::optional<url::Origin> aggregation_coordinator_origin;
};

// InitialInterestGroupUpdateInfo contains required fields when the update
// process is initialized, which includes interest_group_key for
// KAnonymity update, update_url for generating update request and
// joining_origin for grouped isolation info.
struct CONTENT_EXPORT InterestGroupUpdateParameter {
  InterestGroupUpdateParameter();
  InterestGroupUpdateParameter(blink::InterestGroupKey k,
                               GURL u,
                               url::Origin o);
  ~InterestGroupUpdateParameter();

  blink::InterestGroupKey interest_group_key;
  GURL update_url;
  url::Origin joining_origin;
};

struct CONTENT_EXPORT InterestGroupKanonUpdateParameter {
  explicit InterestGroupKanonUpdateParameter(base::Time update_time);
  InterestGroupKanonUpdateParameter(const InterestGroupKanonUpdateParameter&) =
      delete;
  InterestGroupKanonUpdateParameter(InterestGroupKanonUpdateParameter&& other);
  ~InterestGroupKanonUpdateParameter();

  base::Time update_time;
  // All an interest group's keys, including those that are not k-anonymous or
  // have never been joined.
  std::vector<std::string> hashed_keys;
  // Which keys have been newly added in the join or update that triggered this
  // k-anon update.
  std::vector<std::string> newly_added_hashed_keys;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_UPDATE_H_
