// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::InterestGroupAdDataView,
                                        blink::InterestGroup::Ad> {
  static const GURL& render_url(const blink::InterestGroup::Ad& ad) {
    return ad.render_url;
  }

  static const absl::optional<std::string>& metadata(
      const blink::InterestGroup::Ad& ad) {
    return ad.metadata;
  }

  static bool Read(blink::mojom::InterestGroupAdDataView data,
                   blink::InterestGroup::Ad* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::SellerCapabilitiesDataView,
                 blink::InterestGroup::SellerCapabilitiesType> {
  static bool allows_interest_group_counts(
      const blink::InterestGroup::SellerCapabilitiesType& capabilities) {
    return capabilities.Has(
        blink::InterestGroup::SellerCapabilities::kInterestGroupCounts);
  }

  static bool allows_latency_stats(
      const blink::InterestGroup::SellerCapabilitiesType& capabilities) {
    return capabilities.Has(
        blink::InterestGroup::SellerCapabilities::kLatencyStats);
  }

  static bool Read(blink::mojom::SellerCapabilitiesDataView data,
                   blink::InterestGroup::SellerCapabilitiesType* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::InterestGroupDataView, blink::InterestGroup> {
  static base::Time expiry(const blink::InterestGroup& interest_group) {
    return interest_group.expiry;
  }

  static const url::Origin& owner(const blink::InterestGroup& interest_group) {
    return interest_group.owner;
  }

  static const std::string& name(const blink::InterestGroup& interest_group) {
    return interest_group.name;
  }

  static double priority(const blink::InterestGroup& interest_group) {
    return interest_group.priority;
  }

  static bool enable_bidding_signals_prioritization(
      const blink::InterestGroup& interest_group) {
    return interest_group.enable_bidding_signals_prioritization;
  }

  static const absl::optional<base::flat_map<std::string, double>>&
  priority_vector(const blink::InterestGroup& interest_group) {
    return interest_group.priority_vector;
  }

  static const absl::optional<base::flat_map<std::string, double>>&
  priority_signals_overrides(const blink::InterestGroup& interest_group) {
    return interest_group.priority_signals_overrides;
  }

  static const absl::optional<
      base::flat_map<url::Origin,
                     blink::InterestGroup::SellerCapabilitiesType>>&
  seller_capabilities(const blink::InterestGroup& interest_group) {
    return interest_group.seller_capabilities;
  }

  static blink::InterestGroup::SellerCapabilitiesType all_sellers_capabilities(
      const blink::InterestGroup& interest_group) {
    return interest_group.all_sellers_capabilities;
  }

  static blink::InterestGroup::ExecutionMode execution_mode(
      const blink::InterestGroup& interest_group) {
    return interest_group.execution_mode;
  }

  static const absl::optional<GURL>& bidding_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.bidding_url;
  }

  static const absl::optional<GURL>& bidding_wasm_helper_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.bidding_wasm_helper_url;
  }

  static const absl::optional<GURL>& daily_update_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.daily_update_url;
  }

  static const absl::optional<GURL>& trusted_bidding_signals_url(
      const blink::InterestGroup& interest_group) {
    return interest_group.trusted_bidding_signals_url;
  }

  static const absl::optional<std::vector<std::string>>&
  trusted_bidding_signals_keys(const blink::InterestGroup& interest_group) {
    return interest_group.trusted_bidding_signals_keys;
  }

  static const absl::optional<std::string>& user_bidding_signals(
      const blink::InterestGroup& interest_group) {
    return interest_group.user_bidding_signals;
  }

  static const absl::optional<std::vector<blink::InterestGroup::Ad>>& ads(
      const blink::InterestGroup& interest_group) {
    return interest_group.ads;
  }

  static const absl::optional<std::vector<blink::InterestGroup::Ad>>&
  ad_components(const blink::InterestGroup& interest_group) {
    return interest_group.ad_components;
  }

  static bool Read(blink::mojom::InterestGroupDataView data,
                   blink::InterestGroup* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_MOJOM_TRAITS_H_
