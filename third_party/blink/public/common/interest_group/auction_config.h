// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_H_

#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/seller_capabilities.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Refers to a resource in a subresource bundle. Valid only as long as the
// <script type="webbundle"> tag that owns the subresource exists.
struct BLINK_COMMON_EXPORT DirectFromSellerSignalsSubresource {
  DirectFromSellerSignalsSubresource();
  DirectFromSellerSignalsSubresource(const DirectFromSellerSignalsSubresource&);
  DirectFromSellerSignalsSubresource(DirectFromSellerSignalsSubresource&&);
  ~DirectFromSellerSignalsSubresource();

  DirectFromSellerSignalsSubresource& operator=(
      const DirectFromSellerSignalsSubresource&);
  DirectFromSellerSignalsSubresource& operator=(
      DirectFromSellerSignalsSubresource&&);

  GURL bundle_url;
  base::UnguessableToken token;
};

bool BLINK_COMMON_EXPORT
operator==(const DirectFromSellerSignalsSubresource& a,
           const DirectFromSellerSignalsSubresource& b);

// The set of directFromSellerSignals for a particular auction or component
// auction.
struct BLINK_COMMON_EXPORT DirectFromSellerSignals {
  DirectFromSellerSignals();
  DirectFromSellerSignals(const DirectFromSellerSignals&);
  DirectFromSellerSignals(DirectFromSellerSignals&&);
  ~DirectFromSellerSignals();

  DirectFromSellerSignals& operator=(const DirectFromSellerSignals&);
  DirectFromSellerSignals& operator=(DirectFromSellerSignals&&);

  GURL prefix;
  base::flat_map<url::Origin, DirectFromSellerSignalsSubresource>
      per_buyer_signals;
  absl::optional<DirectFromSellerSignalsSubresource> seller_signals;
  absl::optional<DirectFromSellerSignalsSubresource> auction_signals;
};

// AuctionConfig class used by FLEDGE auctions. Typemapped to
// blink::mojom::AuctionAdConfig, primarily so the typemap can include validity
// checks on the origins of the provided URLs. Not called blink::AuctionConfig
// because a class of that name is already created from auction_ad_config.idl.
//
// All URLs and origins must be HTTPS.
struct BLINK_COMMON_EXPORT AuctionConfig {
  // Representation of some auction config field being potentially delivered via
  // a promise, or as its value (tag() == kValue). In case of a promise the
  // resolved value will be delivered later via a Resolved*Promise message on
  // AbortableAdAuction interface. Each specialization is individually
  // typemapped.
  template <typename Value>
  class BLINK_COMMON_EXPORT MaybePromise {
   public:
    using ValueType = Value;

    enum class Tag { kPromise, kValue };

    bool is_promise() const { return tag_ == Tag::kPromise; }

    static MaybePromise<Value> FromPromise() {
      MaybePromise<Value> result;
      result.tag_ = Tag::kPromise;
      return result;
    }

    static MaybePromise<Value> FromValue(Value value_in) {
      MaybePromise<Value> result;
      result.value_ = std::move(value_in);
      result.tag_ = Tag::kValue;
      return result;
    }

    Tag tag() const { return tag_; }
    const Value& value() const { return value_; }
    Value& mutable_value_for_testing() { return value_; }

   private:
    Tag tag_ = Tag::kValue;
    Value value_;
  };

  // Typemapped to blink::mojom::AuctionAdConfigMaybePromiseJson
  using MaybePromiseJson = MaybePromise<absl::optional<std::string>>;

  // Typemapped to blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignals.
  using MaybePromisePerBuyerSignals =
      MaybePromise<absl::optional<base::flat_map<url::Origin, std::string>>>;

  // Typemapped to
  // blink::mojom::AuctionAdConfigMaybePromiseDirectFromSellerSignals
  using MaybePromiseDirectFromSellerSignals =
      MaybePromise<absl::optional<DirectFromSellerSignals>>;

  // Representation of bidder timeouts, including optional global and per-origin
  // timeouts.
  //
  // Typemapped to blink::mojom::AuctionAdConfigBuyerTimeouts.
  struct BuyerTimeouts {
    // The value restricts generateBid() script's runtime of all buyers with
    // unspecified timeouts, if present.
    absl::optional<base::TimeDelta> all_buyers_timeout;

    // Values restrict the runtime of particular buyer's generateBid() scripts.
    absl::optional<base::flat_map<url::Origin, base::TimeDelta>>
        per_buyer_timeouts;
  };

  // Typemapped to blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeouts
  using MaybePromiseBuyerTimeouts = MaybePromise<BuyerTimeouts>;

  // Subset of AuctionConfig that is not shared by all auctions that are
  // using the same SellerWorklet object (so it's "not shared" between
  // AuctionConfigs that share the same SellerWorklet). Other AuctionConfig
  // parameters all must be the same for two auctions to share a Sellerworklet.
  //
  // Typemapped to blink::mojom::AuctionAdConfigNonSharedParams.
  struct BLINK_COMMON_EXPORT NonSharedParams {
    // For each report requested by the seller, this enum specifies the type of
    // the report.
    using BuyerReportType =
        blink::mojom::AuctionAdConfigNonSharedParams_BuyerReportType;

    // For each report type, provides the bucket offset and scalar multiplier
    // for that report.
    //
    // Typemapped to blink::mojom::AuctionReportBuyersConfig.
    struct BLINK_COMMON_EXPORT AuctionReportBuyersConfig {
      // The bucket offset, added to the base per-buyer bucket value to obtain
      // the actual bucket number used for reporting.
      absl::uint128 bucket;

      // A scalar multiplier multiplied by the reported value, to control the
      // amount of noise added by the aggregation service. (Reading aggreaged
      // reported values is subject to a privacy budget, so this controls how
      // much budget is spent on each report).
      double scale;
    };

    NonSharedParams();
    NonSharedParams(const NonSharedParams&);
    NonSharedParams(NonSharedParams&&);
    ~NonSharedParams();

    NonSharedParams& operator=(const NonSharedParams&);
    NonSharedParams& operator=(NonSharedParams&&);

    // Owners of interest groups allowed to participate in the auction.
    absl::optional<std::vector<url::Origin>> interest_group_buyers;

    // Opaque JSON data, passed as object to all worklets. This can be a promise
    // when renderer is talking to browser, but will be resolved before passing
    // to worklet.
    MaybePromiseJson auction_signals;

    // Opaque JSON data, passed as object to the seller worklet. This can be a
    // promise when renderer is talking to browser, but will be resolved before
    // passing to worklet.
    MaybePromiseJson seller_signals;

    // The value restricts the runtime of the seller's scoreAd() script.
    absl::optional<base::TimeDelta> seller_timeout;

    // Value is opaque JSON data, passed as object to particular buyers.
    MaybePromisePerBuyerSignals per_buyer_signals;

    // Values restrict the runtime of generateBid() scripts.
    MaybePromiseBuyerTimeouts buyer_timeouts;

    // Collective timeouts for all interest groups with the same buyer. Includes
    // launching worklet processes, loading scripts and signals, and running
    // the buyer's generateBid() functions.
    MaybePromiseBuyerTimeouts buyer_cumulative_timeouts;

    // Values restrict the number of bidding interest groups for a particular
    // buyer that can participate in an auction. Values must be greater than 0.
    base::flat_map<url::Origin, std::uint16_t> per_buyer_group_limits;

    // Limit on the number of bidding interest groups for any buyer. Must be
    // greater than 0. Defaults to the largest uint16 value, which is fine
    // in our case since the backend storage applies a lower limit.
    std::uint16_t all_buyers_group_limit =
        std::numeric_limits<std::uint16_t>::max();

    // Per-buyer sparse vector that, along with a similar per-interest group
    // sparse vector, has its dot product taken to calculate interest group
    // priorities.
    absl::optional<
        base::flat_map<url::Origin, base::flat_map<std::string, double>>>
        per_buyer_priority_signals;

    // Merged with `per_buyer_priority_signals` before calculating
    // per-interest group priorities. In the case both have entries with the
    // same key, the entry in `per_buyer_priority_signals` takes precedence.
    absl::optional<base::flat_map<std::string, double>>
        all_buyers_priority_signals;

    // For each buyer in `interest_group_buyers`, specifies the base bucket ID
    // number for that buyer. To be used in conjunction with
    // `auction_report_buyers`; for each buyer, for each report type, the
    // base bucket ID is added to the `auction_report_buyers` bucket offset to
    // obtain the actual bucket numbers used for reporting.
    absl::optional<std::vector<absl::uint128>> auction_report_buyer_keys;

    // For each type of bidder extended private aggregation reporting event,
    // provides the bucket offset and scalar multiplier for that event.
    absl::optional<base::flat_map<BuyerReportType, AuctionReportBuyersConfig>>
        auction_report_buyers;

    // The set of seller capabilities that each interest group must declare in
    // order to participate in the auction. Interest groups that don't declare
    // all required seller capabilities will not participate in the auction.
    SellerCapabilitiesType required_seller_capabilities;

    // Nested auctions whose results will also be fed to `seller`. Only the top
    // level auction config can have component auctions.
    std::vector<AuctionConfig> component_auctions;
  };

  AuctionConfig();
  AuctionConfig(const AuctionConfig&);
  AuctionConfig(AuctionConfig&&);
  ~AuctionConfig();

  AuctionConfig& operator=(const AuctionConfig&);
  AuctionConfig& operator=(AuctionConfig&&);

  // Returns how many of the params are promises. Includes component auctions.
  int NumPromises() const;

  // Helper to check if `url` is HTTPS and matches origin of `seller`.
  bool IsHttpsAndMatchesSellerOrigin(const GURL& url) const;

  // Helper to verify if DirectFromSellerSignals is valid in context of this
  // auction.
  bool IsDirectFromSellerSignalsValid(
      const absl::optional<blink::DirectFromSellerSignals>&
          direct_from_seller_signals) const;

  // Seller running the auction.
  url::Origin seller;

  // Both URLS, if present, must be same-origin to `seller`.
  GURL decision_logic_url;
  absl::optional<GURL> trusted_scoring_signals_url;

  // Other parameters are grouped in a struct that is passed to SellerWorklets.
  NonSharedParams non_shared_params;

  // Subresource bundle URLs that when fetched should yield a JSON
  // direct_from_seller_signals responses for the seller and buyers.
  MaybePromiseDirectFromSellerSignals direct_from_seller_signals;

  // Identifier for an experiment group, used when getting trusted
  // signals (and as part of AuctionConfig given to worklets).
  absl::optional<uint16_t> seller_experiment_group_id;
  absl::optional<uint16_t> all_buyer_experiment_group_id;
  base::flat_map<url::Origin, uint16_t> per_buyer_experiment_group_ids;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_H_
