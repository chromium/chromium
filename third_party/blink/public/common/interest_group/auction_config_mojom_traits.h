// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-shared.h"

class GURL;

namespace url {

class Origin;

}  // namespace url

namespace base {

class UnguessableToken;

}  // namespace base

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::DirectFromSellerSignalsSubresourceDataView,
                 blink::DirectFromSellerSignalsSubresource> {
  static const GURL& bundle_url(
      const blink::DirectFromSellerSignalsSubresource& params) {
    return params.bundle_url;
  }

  static const base::UnguessableToken& token(
      const blink::DirectFromSellerSignalsSubresource& params) {
    return params.token;
  }

  static bool Read(
      blink::mojom::DirectFromSellerSignalsSubresourceDataView data,
      blink::DirectFromSellerSignalsSubresource* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::DirectFromSellerSignalsDataView,
                 blink::DirectFromSellerSignals> {
  static const GURL& prefix(const blink::DirectFromSellerSignals& params) {
    return params.prefix;
  }

  static const base::flat_map<url::Origin,
                              blink::DirectFromSellerSignalsSubresource>&
  per_buyer_signals(const blink::DirectFromSellerSignals& params) {
    return params.per_buyer_signals;
  }

  static const absl::optional<blink::DirectFromSellerSignalsSubresource>&
  seller_signals(const blink::DirectFromSellerSignals& params) {
    return params.seller_signals;
  }

  static const absl::optional<blink::DirectFromSellerSignalsSubresource>&
  auction_signals(const blink::DirectFromSellerSignals& params) {
    return params.auction_signals;
  }

  static bool Read(blink::mojom::DirectFromSellerSignalsDataView data,
                   blink::DirectFromSellerSignals* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::AuctionAdConfigMaybePromiseJsonDataView,
                blink::AuctionConfig::MaybePromiseJson> {
  static blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag GetTag(
      const blink::AuctionConfig::MaybePromiseJson& value) {
    switch (value.tag()) {
      case blink::AuctionConfig::MaybePromiseJson::Tag::kNothing:
        return blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::
            kNothing;
      case blink::AuctionConfig::MaybePromiseJson::Tag::kPromise:
        return blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::
            kPromise;
      case blink::AuctionConfig::MaybePromiseJson::Tag::kJson:
        return blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::
            kJson;
    }
    NOTREACHED();
    return blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::kNothing;
  }

  static uint32_t nothing(const blink::AuctionConfig::MaybePromiseJson& value) {
    return 0u;  // Ignored placeholder value.
  }

  static uint32_t promise(const blink::AuctionConfig::MaybePromiseJson& value) {
    return 0u;  // Ignored placeholder value.
  }

  static const std::string& json(
      const blink::AuctionConfig::MaybePromiseJson& value) {
    return value.json_payload();
  }

  static bool Read(blink::mojom::AuctionAdConfigMaybePromiseJsonDataView in,
                   blink::AuctionConfig::MaybePromiseJson* out);
};

template <>
struct BLINK_COMMON_EXPORT UnionTraits<
    blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView,
    blink::AuctionConfig::MaybePromisePerBuyerSignals> {
  static blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView::Tag
  GetTag(const blink::AuctionConfig::MaybePromisePerBuyerSignals& value) {
    switch (value.tag()) {
      case blink::AuctionConfig::MaybePromisePerBuyerSignals::Tag::kPromise:
        return blink::mojom::
            AuctionAdConfigMaybePromisePerBuyerSignalsDataView::Tag::kPromise;
      case blink::AuctionConfig::MaybePromisePerBuyerSignals::Tag::
          kPerBuyerSignals:
        return blink::mojom::
            AuctionAdConfigMaybePromisePerBuyerSignalsDataView::Tag::
                kPerBuyerSignals;
    }
    NOTREACHED();
    return blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView::
        Tag::kPerBuyerSignals;
  }

  static uint32_t promise(
      const blink::AuctionConfig::MaybePromisePerBuyerSignals& value) {
    return 0u;  // Ignored placeholder value.
  }

  static const absl::optional<base::flat_map<url::Origin, std::string>>&
  per_buyer_signals(
      const blink::AuctionConfig::MaybePromisePerBuyerSignals& value) {
    return value.value();
  }

  static bool Read(
      blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView in,
      blink::AuctionConfig::MaybePromisePerBuyerSignals* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AuctionAdConfigBuyerTimeoutsDataView,
                 blink::AuctionConfig::BuyerTimeouts> {
  static const absl::optional<base::flat_map<url::Origin, base::TimeDelta>>&
  per_buyer_timeouts(const blink::AuctionConfig::BuyerTimeouts& params) {
    return params.per_buyer_timeouts;
  }

  static const absl::optional<base::TimeDelta>& all_buyers_timeout(
      const blink::AuctionConfig::BuyerTimeouts& params) {
    return params.all_buyers_timeout;
  }

  static bool Read(blink::mojom::AuctionAdConfigBuyerTimeoutsDataView data,
                   blink::AuctionConfig::BuyerTimeouts* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView,
                blink::AuctionConfig::MaybePromiseBuyerTimeouts> {
  static blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::Tag
  GetTag(const blink::AuctionConfig::MaybePromiseBuyerTimeouts& value) {
    switch (value.tag()) {
      case blink::AuctionConfig::MaybePromiseBuyerTimeouts::Tag::kPromise:
        return blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::
            Tag::kPromise;
      case blink::AuctionConfig::MaybePromiseBuyerTimeouts::Tag::kValue:
        return blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::
            Tag::kValue;
    }
    NOTREACHED();
    return blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::Tag::
        kValue;
  }

  static uint32_t promise(
      const blink::AuctionConfig::MaybePromiseBuyerTimeouts& value) {
    return 0u;  // Ignored placeholder value.
  }

  static const blink::AuctionConfig::BuyerTimeouts& value(
      const blink::AuctionConfig::MaybePromiseBuyerTimeouts& value) {
    return value.value();
  }

  static bool Read(
      blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView in,
      blink::AuctionConfig::MaybePromiseBuyerTimeouts* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::AuctionReportBuyersConfigDataView,
    blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig> {
  static absl::uint128 bucket(
      const blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig&
          params) {
    return params.bucket;
  }

  static double scale(
      const blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig&
          params) {
    return params.scale;
  }

  static bool Read(
      blink::mojom::AuctionReportBuyersConfigDataView data,
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AuctionAdConfigNonSharedParamsDataView,
                 blink::AuctionConfig::NonSharedParams> {
  static const absl::optional<std::vector<url::Origin>>& interest_group_buyers(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.interest_group_buyers;
  }

  static const blink::AuctionConfig::MaybePromiseJson& auction_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.auction_signals;
  }

  static const blink::AuctionConfig::MaybePromiseJson& seller_signals(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.seller_signals;
  }

  static absl::optional<base::TimeDelta> seller_timeout(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.seller_timeout;
  }

  static const blink::AuctionConfig::MaybePromisePerBuyerSignals&
  per_buyer_signals(const blink::AuctionConfig::NonSharedParams& params) {
    return params.per_buyer_signals;
  }

  static const blink::AuctionConfig::MaybePromiseBuyerTimeouts& buyer_timeouts(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.buyer_timeouts;
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

  static const absl::optional<std::vector<absl::uint128>>&
  auction_report_buyer_keys(
      const blink::AuctionConfig::NonSharedParams& params) {
    return params.auction_report_buyer_keys;
  }

  static const absl::optional<base::flat_map<
      blink::AuctionConfig::NonSharedParams::BuyerReportType,
      blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>>&
  auction_report_buyers(const blink::AuctionConfig::NonSharedParams& params) {
    return params.auction_report_buyers;
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

  static const absl::optional<blink::DirectFromSellerSignals>&
  direct_from_seller_signals(const blink::AuctionConfig& params) {
    return params.direct_from_seller_signals;
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
