// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_TEST_INTEREST_GROUP_BUILDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_TEST_INTEREST_GROUP_BUILDER_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Test-only single-use builder for interest groups. Uses empty defaults, and a
// 30 day (from construction time) expiry.
class TestInterestGroupBuilder {
 public:
  TestInterestGroupBuilder(url::Origin owner, std::string name);

  ~TestInterestGroupBuilder();

  InterestGroup Build();

  TestInterestGroupBuilder& SetExpiry(base::Time expiry);
  TestInterestGroupBuilder& SetPriority(double priority);
  TestInterestGroupBuilder& SetEnableBiddingSignalsPrioritization(
      bool enable_bidding_signals_prioritization);
  TestInterestGroupBuilder& SetPriorityVector(
      absl::optional<base::flat_map<std::string, double>> priority_vector);
  TestInterestGroupBuilder& SetPrioritySignalsOverrides(
      absl::optional<base::flat_map<std::string, double>>
          priority_signals_overrides);
  TestInterestGroupBuilder& SetSellerCapabilities(
      absl::optional<
          base::flat_map<url::Origin, InterestGroup::SellerCapabilitiesType>>
          seller_capabilities);
  TestInterestGroupBuilder& SetAllSellerCapabilities(
      InterestGroup::SellerCapabilitiesType all_sellers_capabilities);
  TestInterestGroupBuilder& SetExecutionMode(
      InterestGroup::ExecutionMode execution_mode);
  TestInterestGroupBuilder& SetBiddingUrl(absl::optional<GURL> bidding_url);
  TestInterestGroupBuilder& SetBiddingWasmHelperUrl(
      absl::optional<GURL> bidding_wasm_helper_url);
  TestInterestGroupBuilder& SetDailyUpdateUrl(
      absl::optional<GURL> daily_update_url);
  TestInterestGroupBuilder& SetTrustedBiddingSignalsUrl(
      absl::optional<GURL> trusted_bidding_signals_url);
  TestInterestGroupBuilder& SetTrustedBiddingSignalsKeys(
      absl::optional<std::vector<std::string>> trusted_bidding_signals_keys);
  TestInterestGroupBuilder& SetUserBiddingSignals(
      absl::optional<std::string> user_bidding_signals);
  TestInterestGroupBuilder& SetAds(
      absl::optional<std::vector<InterestGroup::Ad>> ads);
  TestInterestGroupBuilder& SetAdComponentss(
      absl::optional<std::vector<InterestGroup::Ad>> ad_components);

 private:
  InterestGroup interest_group_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_TEST_INTEREST_GROUP_BUILDER_H_
