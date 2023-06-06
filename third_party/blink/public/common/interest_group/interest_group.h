// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/seller_capabilities.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Interest group used by FLEDGE auctions. Typemapped to
// blink::mojom::InterestGroup, primarily so the typemap can include validity
// checks on the origins of the provided URLs.
//
// All URLs and origins must use https, and same origin to `owner`.
//
// https://github.com/WICG/turtledove/blob/main/FLEDGE.md#11-joining-interest-groups
struct BLINK_COMMON_EXPORT InterestGroup {
  using ExecutionMode = blink::mojom::InterestGroup_ExecutionMode;
  // An advertisement to display for an interest group. Typemapped to
  // blink::mojom::InterestGroupAd.
  // https://github.com/WICG/turtledove/blob/main/FLEDGE.md#12-interest-group-attributes
  struct BLINK_COMMON_EXPORT Ad {
    Ad();
    Ad(GURL render_url,
       absl::optional<std::string> metadata,
       absl::optional<std::string> size_group = absl::nullopt,
       absl::optional<std::string> buyer_reporting_id = absl::nullopt,
       absl::optional<std::string> buyer_and_seller_reporting_id =
           absl::nullopt,
       absl::optional<std::string> ad_render_id = absl::nullopt);
    ~Ad();

    // Returns the approximate size of the contents of this InterestGroup::Ad,
    // in bytes.
    size_t EstimateSize() const;

    // Must use https.
    GURL render_url;
    // Optional size group assigned to this Ad.
    absl::optional<std::string> size_group;
    // Opaque JSON data, passed as an object to auction worklet.
    absl::optional<std::string> metadata;

    // Optional alternative identifiers for reporting purposes that can be
    // passed to reporting scripts in lieu of group name if they pass k-anon
    // checks.
    absl::optional<std::string> buyer_reporting_id;
    absl::optional<std::string> buyer_and_seller_reporting_id;

    // Optional alias to use for B&A auctions
    absl::optional<std::string> ad_render_id;

    // Only used in tests, but provided as an operator instead of as
    // IsEqualForTesting() to make it easier to implement InterestGroup's
    // IsEqualForTesting().
    bool operator==(const Ad& other) const;
  };

  InterestGroup();

  // Constructor takes arguments by value. They're unlikely to be independently
  // useful at the point of construction, so caller can std::move() them when
  // invoking the constructor.
  InterestGroup(
      base::Time expiry,
      url::Origin owner,
      std::string name,
      double priority,
      bool enable_bidding_signals_prioritization,
      absl::optional<base::flat_map<std::string, double>> priority_vector,
      absl::optional<base::flat_map<std::string, double>>
          priority_signals_overrides,
      absl::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
          seller_capabilities,
      SellerCapabilitiesType all_sellers_capabilities,
      ExecutionMode execution_mode,
      absl::optional<GURL> bidding_url,
      absl::optional<GURL> bidding_wasm_helper_url,
      absl::optional<GURL> update_url,
      absl::optional<GURL> trusted_bidding_signals_url,
      absl::optional<std::vector<std::string>> trusted_bidding_signals_keys,
      absl::optional<std::string> user_bidding_signals,
      absl::optional<std::vector<InterestGroup::Ad>> ads,
      absl::optional<std::vector<InterestGroup::Ad>> ad_components,
      absl::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes,
      absl::optional<base::flat_map<std::string, std::vector<std::string>>>
          size_groups);

  ~InterestGroup();

  // Checks for validity. Performs same checks as IsBlinkInterestGroupValid().
  // Automatically checked when passing InterestGroups over Mojo.
  bool IsValid() const;

  // Returns the approximate size of the contents of this InterestGroup, in
  // bytes.
  size_t EstimateSize() const;

  bool IsEqualForTesting(const InterestGroup& other) const;

  base::Time expiry;
  url::Origin owner;
  std::string name;

  double priority = 0;
  bool enable_bidding_signals_prioritization = false;
  absl::optional<base::flat_map<std::string, double>> priority_vector;
  absl::optional<base::flat_map<std::string, double>>
      priority_signals_overrides;

  absl::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
      seller_capabilities;
  SellerCapabilitiesType all_sellers_capabilities;
  ExecutionMode execution_mode = ExecutionMode::kCompatibilityMode;
  absl::optional<GURL> bidding_url;
  absl::optional<GURL> bidding_wasm_helper_url;
  absl::optional<GURL> update_url;
  absl::optional<GURL> trusted_bidding_signals_url;
  absl::optional<std::vector<std::string>> trusted_bidding_signals_keys;
  absl::optional<std::string> user_bidding_signals;
  absl::optional<std::vector<InterestGroup::Ad>> ads, ad_components;
  absl::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes;
  absl::optional<base::flat_map<std::string, std::vector<std::string>>>
      size_groups;

  static_assert(__LINE__ == 142, R"(
If modifying InterestGroup fields, make sure to also modify:

* IsValid(), EstimateSize(), and IsEqualForTesting() in this class
* auction_ad_interest_group.idl
* navigator_auction.cc
* interest_group_types.mojom
* validate_blink_interest_group.cc
* validate_blink_interest_group_test.cc
* test_interest_group_builder[.h/.cc]
* interest_group_mojom_traits[.h/.cc/.test].
* bidder_worklet.cc (to pass the InterestGroup to generateBid()).

In interest_group_storage.cc, add the new field and any respective indices,
update `ClearExcessiveStorage()`, add a new database version and migration, and
migration test.

If the new field is to be updatable via dailyUpdateUrl, also update *all* of
these:

* Add field to content::InterestGroupUpdate.
* InterestGroupStorage::DoStoreInterestGroupUpdate()
* ParseUpdateJson in interest_group_update_manager.cc
* Update AdAuctionServiceImplTest.UpdateAllUpdatableFields

See crrev.com/c/3517534 for an example (adding the priority field), and also
remember to update bidder_worklet.cc too.

If the new field should be sent to the B&A server for server-side auctions then
SerializeInterestGroup() in bidding_and_auction_serializer.cc needs modified to
support the new field.
)");
};

// A unique identifier for interest groups.
struct InterestGroupKey {
  InterestGroupKey(url::Origin o, std::string n)
      : owner(std::move(o)), name(std::move(n)) {}
  inline bool operator<(const InterestGroupKey& other) const {
    return owner != other.owner ? owner < other.owner : name < other.name;
  }
  inline bool operator==(const InterestGroupKey& other) const {
    return owner == other.owner && name == other.name;
  }
  url::Origin owner;
  std::string name;
};

// A set of interest groups, identified by owner and name. Used to log which
// interest groups bid in an auction. A sets is used to avoid double-counting
// interest groups that bid in multiple components auctions in a component
// auction.
using InterestGroupSet = std::set<InterestGroupKey>;

// Calculates the k-anonymity key for an Ad that is used for determining if an
// ad is k-anonymous for the purposes of bidding and winning an auction.
// We want to avoid providing too much identifying information for event level
// reporting in reportWin. This key is used to check that providing the interest
// group owner and ad URL to the bidding script doesn't identify the user. It is
// used to gate whether an ad can participate in a FLEDGE auction because event
// level reports need to include both the owner and ad URL for the purposes of
// an auction.
std::string BLINK_COMMON_EXPORT KAnonKeyForAdBid(const InterestGroup& group,
                                                 const GURL& ad_url);
std::string BLINK_COMMON_EXPORT
KAnonKeyForAdBid(const InterestGroup& group,
                 const blink::AdDescriptor& ad_descriptor);
std::string BLINK_COMMON_EXPORT KAnonKeyForAdBid(const url::Origin& owner,
                                                 const GURL& bidding_url,
                                                 const GURL& ad_url);
std::string BLINK_COMMON_EXPORT
KAnonKeyForAdBid(const url::Origin& owner,
                 const GURL& bidding_url,
                 const blink::AdDescriptor& ad_descriptor);

// Calculates the k-anonymity key for an ad component that is used for
// determining if an ad component is k-anonymous for the purposes of bidding and
// winning an auction. Since ad components are not provided to reporting, we
// only are concerned with micro-targetting. This means we can just use the ad
// url as the k-anonymity key.
std::string BLINK_COMMON_EXPORT KAnonKeyForAdComponentBid(const GURL& ad_url);
std::string BLINK_COMMON_EXPORT
KAnonKeyForAdComponentBid(const blink::AdDescriptor& ad_descriptor);

// Calculates the k-anonymity key for reporting the interest group name in
// reportWin along with the given Ad.
// We want to avoid providing too much identifying information for event level
// reporting in reportWin. This key is used to check if including the passed in
// identifier --- either a specific explicit campaign ID or, as fallback
// the interest group name  --- along with the interest group owner and ad URL
// would make the user too identifiable. If this key is not k-anonymous then we
// do not provide the interest group name to reportWin.
std::string BLINK_COMMON_EXPORT
KAnonKeyForAdNameReporting(const InterestGroup& group,
                           const InterestGroup::Ad& ad);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_INTEREST_GROUP_H_
