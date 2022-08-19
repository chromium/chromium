// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// AuctionConfig class used by FLEDGE auctions. Typemapped to
// blink::mojom::AuctionAdConfig, primarily so the typemap can include validity
// checks on the origins of the provided URLs. Not called blink::AuctionConfig
// because a class of that name is already created from auction_ad_config.idl.
//
// All URLs and origins must be HTTPS.
struct BLINK_COMMON_EXPORT AuctionConfig {
  // Subset of AuctionConfig that is not shared by all auctions that are
  // using the same SellerWorklet object (so it's "not shared" between
  // AuctionConfigs that share the same SellerWorklet). Other AuctionConfig
  // parameters all must be the same for two auctions to share a Sellerworklet.
  //
  // Typemapped to blink::mojom::AuctionAdConfigNonSharedParams.
  struct BLINK_COMMON_EXPORT NonSharedParams {
    NonSharedParams();
    NonSharedParams(const NonSharedParams&);
    NonSharedParams(NonSharedParams&&);
    ~NonSharedParams();

    NonSharedParams& operator=(const NonSharedParams&);
    NonSharedParams& operator=(NonSharedParams&&);

    // Owners of interest groups allowed to participate in the auction.
    absl::optional<std::vector<url::Origin>> interest_group_buyers;

    // Opaque JSON data, passed as object to all worklets.
    absl::optional<std::string> auction_signals;

    // Opaque JSON data, passed as object to the seller worklet.
    absl::optional<std::string> seller_signals;

    // The value restricts the runtime of the seller's scoreAd() script.
    absl::optional<base::TimeDelta> seller_timeout;

    // Value is opaque JSON data, passed as object to particular buyers.
    absl::optional<base::flat_map<url::Origin, std::string>> per_buyer_signals;

    // Values restrict the runtime of particular buyer's generateBid() scripts.
    absl::optional<base::flat_map<url::Origin, base::TimeDelta>>
        per_buyer_timeouts;

    // The value restricts generateBid() script's runtime of all buyers with
    // unspecified timeouts, if present.
    absl::optional<base::TimeDelta> all_buyers_timeout;

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

  // Seller running the auction.
  url::Origin seller;

  // Both URLS, if present, must be same-origin to `seller`.
  GURL decision_logic_url;
  absl::optional<GURL> trusted_scoring_signals_url;

  // Other parameters are grouped in a struct that is passed to SellerWorklets.
  NonSharedParams non_shared_params;

  // Identifier for an experiment group, used when getting trusted
  // signals (and as part of AuctionConfig given to worklets).
  absl::optional<uint16_t> seller_experiment_group_id;
  absl::optional<uint16_t> all_buyer_experiment_group_id;
  base::flat_map<url::Origin, uint16_t> per_buyer_experiment_group_ids;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AUCTION_CONFIG_H_
