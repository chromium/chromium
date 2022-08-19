// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AuctionAdConfigNonSharedParamsDataView,
                 blink::AuctionConfig::NonSharedParams> {
  static const absl::optional<std::vector<url::Origin>>& interest_group_buyers(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.interest_group_buyers;
  }

  static const absl::optional<std::string>& auction_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.auction_signals;
  }

  static const absl::optional<std::string>& seller_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.seller_signals;
  }

  static absl::optional<base::TimeDelta> seller_timeout(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.seller_timeout;
  }

  static const absl::optional<base::flat_map<url::Origin, std::string>>&
  per_buyer_signals(const blink::AuctionConfig::NonSharedParams& params) {
    return params.per_buyer_signals;
  }

  static const absl::optional<base::flat_map<url::Origin, base::TimeDelta>>&
  per_buyer_timeouts(const blink::AuctionConfig::NonSharedParams& params) {
    return params.per_buyer_timeouts;
  }

  static const absl::optional<base::TimeDelta>& all_buyers_timeout(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.all_buyers_timeout;
  }

  static const base::flat_map<url::Origin, std::uint16_t>&
  per_buyer_group_limits(const blink::AuctionConfig::NonSharedParams& params) {
    return params.per_buyer_group_limits;
  }

  static std::uint16_t all_buyers_group_limit(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.all_buyers_group_limit;
  }

  static const absl::optional<
      base::flat_map<url::Origin, base::flat_map<std::string, double>>>&
  per_buyer_priority_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.per_buyer_priority_signals;
  }

  static const absl::optional<base::flat_map<std::string, double>>&
  all_buyers_priority_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.all_buyers_priority_signals;
  }

  static const std::vector<blink::AuctionConfig>& component_auctions(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.component_auctions;
  }

  static bool Read(blink::mojom::AuctionAdConfigNonSharedParamsDataView data,
                   blink::AuctionConfig::NonSharedParams* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AuctionAdConfigDataView, blink::AuctionConfig> {
  static const url::Origin& seller(const blink::AuctionConfig& config) {
    return config.seller;
  }

  static const GURL& decision_logic_url(const blink::AuctionConfig& config) {
    return config.decision_logic_url;
  }

  static const absl::optional<GURL>& trusted_scoring_signals_url(
      const blink::AuctionConfig& config) {
    return config.trusted_scoring_signals_url;
  }

  static const blink::AuctionConfig::NonSharedParams&
  auction_ad_config_non_shared_params(const blink::AuctionConfig& config) {
    return config.non_shared_params;
  }

  static bool has_seller_experiment_group_id(
      const blink::AuctionConfig& config) {
    return config.seller_experiment_group_id.has_value();
  }

  static std::int16_t seller_experiment_group_id(
      const blink::AuctionConfig& config) {
    return config.seller_experiment_group_id.value_or(0);
  }

  static bool has_all_buyer_experiment_group_id(
      const blink::AuctionConfig& config) {
    return config.all_buyer_experiment_group_id.has_value();
  }

  static std::int16_t all_buyer_experiment_group_id(
      const blink::AuctionConfig& config) {
    return config.all_buyer_experiment_group_id.value_or(0);
  }

  static const base::flat_map<url::Origin, uint16_t>&
  per_buyer_experiment_group_ids(const blink::AuctionConfig& config) {
    return config.per_buyer_experiment_group_ids;
  }

  static bool Read(blink::mojom::AuctionAdConfigDataView data,
                   blink::AuctionConfig* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_MOJOM_TRAITS_H_
