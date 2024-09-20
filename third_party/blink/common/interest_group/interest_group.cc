// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include <stdint.h>

#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blink {

namespace {

constexpr char kKAnonKeyForAdNameReportingSelectedBuyerAndSellerIdPrefix[] =
    "SelectedBuyerAndSellerReportId\n";
constexpr char kKAnonKeyForAdNameReportingBuyerAndSellerIdPrefix[] =
    "BuyerAndSellerReportId\n";
constexpr char kKAnonKeyForAdNameReportingBuyerReportIdPrefix[] =
    "BuyerReportId\n";
constexpr char kKAnonKeyForAdNameReportingNamePrefix[] = "NameReport\n";

const size_t kMaxAdRenderIdSize = 12;

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  return !url.has_username() && !url.has_password();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, update URL. Ad render URLs should be checked with
// IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check, and allows references.
bool IsUrlAllowed(const GURL& url, const InterestGroup& group) {
  if (url::Origin::Create(url) != group.owner) {
    return false;
  }

  return IsUrlAllowedForRenderUrls(url) && !url.has_ref();
}

// Check if `url` can be used with the specified interest group for trusted
// bidding signals URL.
bool IsUrlAllowedForTrustedBiddingSignals(const GURL& url,
                                          const InterestGroup& group) {
  if (!IsUrlAllowedForRenderUrls(url) || url.has_ref() || url.has_query()) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgePermitCrossOriginTrustedSignals)) {
    return true;
  } else {
    return url::Origin::Create(url) == group.owner;
  }
}

size_t EstimateFlatMapSize(
    const base::flat_map<std::string, double>& flat_map) {
  size_t result = 0;
  for (const auto& pair : flat_map) {
    result += pair.first.length() + sizeof(pair.second);
  }
  return result;
}

void AppendReportingIdForSelectedReportingKeyKAnonKey(
    base::optional_ref<const std::string> reporting_id,
    std::string& k_anon_key) {
  if (!reporting_id.has_value()) {
    base::StrAppend(&k_anon_key,
                    {"\n", std::string_view("\x00\x00\x00\x00\x00", 5)});
    return;
  }

  std::array<uint8_t, 4u> size_in_bytes =
      base::U32ToBigEndian(reporting_id->size());
  base::StrAppend(
      &k_anon_key,
      {"\n", std::string_view("\x01", 1),
       base::as_string_view(base::as_chars(base::make_span(size_in_bytes))),
       *reporting_id});
}

std::string InternalPlainTextKAnonKeyForAdNameReporting(
    const url::Origin& interest_group_owner,
    const std::string& interest_group_name,
    const GURL& interest_group_bidding_url,
    const std::string& ad_render_url,
    base::optional_ref<const std::string> buyer_reporting_id,
    base::optional_ref<const std::string> buyer_and_seller_reporting_id,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id) {
  std::string middle =
      base::StrCat({interest_group_owner.GetURL().spec(), "\n",
                    interest_group_bidding_url.spec(), "\n", ad_render_url});

  if (selected_buyer_and_seller_reporting_id.has_value()) {
    // In the case where the reporting functions get
    // `selected_buyer_and_seller_reporting_id`, it's possible for more than one
    // reporting id to be provided. As such, all of the reporting ids passed
    // into the reporting functions will need to be in the k-anon key. To ensure
    // that the k-anon key uniquely reflects the values of all provided
    // reporting ids, we construct the k-anon key so that each reporting id is
    // prefixed by a fixed number of bytes representing its presence and length.
    std::string k_anon_key = base::StrCat(
        {kKAnonKeyForAdNameReportingSelectedBuyerAndSellerIdPrefix, middle});
    AppendReportingIdForSelectedReportingKeyKAnonKey(
        selected_buyer_and_seller_reporting_id, k_anon_key);
    AppendReportingIdForSelectedReportingKeyKAnonKey(
        buyer_and_seller_reporting_id, k_anon_key);
    AppendReportingIdForSelectedReportingKeyKAnonKey(buyer_reporting_id,
                                                     k_anon_key);
    return k_anon_key;
  }

  if (buyer_and_seller_reporting_id.has_value()) {
    return base::StrCat({kKAnonKeyForAdNameReportingBuyerAndSellerIdPrefix,
                         middle, "\n", *buyer_and_seller_reporting_id});
  }

  if (buyer_reporting_id.has_value()) {
    return base::StrCat({kKAnonKeyForAdNameReportingBuyerReportIdPrefix, middle,
                         "\n", *buyer_reporting_id});
  }

  return base::StrCat({kKAnonKeyForAdNameReportingNamePrefix, middle, "\n",
                       interest_group_name});
}
}  // namespace

InterestGroup::Ad::Ad() = default;

InterestGroup::Ad::Ad(base::PassKey<content::InterestGroupStorage>,
                      std::string&& render_url)
    : render_url_(std::move(render_url)) {}
InterestGroup::Ad::Ad(base::PassKey<content::InterestGroupStorage>,
                      const std::string& render_url)
    : render_url_(render_url) {}
InterestGroup::Ad::Ad(
    GURL render_gurl,
    std::optional<std::string> metadata,
    std::optional<std::string> size_group,
    std::optional<std::string> buyer_reporting_id,
    std::optional<std::string> buyer_and_seller_reporting_id,
    std::optional<std::vector<std::string>>
        selectable_buyer_and_seller_reporting_ids,
    std::optional<std::string> ad_render_id,
    std::optional<std::vector<url::Origin>> allowed_reporting_origins)
    : size_group(std::move(size_group)),
      metadata(std::move(metadata)),
      buyer_reporting_id(std::move(buyer_reporting_id)),
      buyer_and_seller_reporting_id(std::move(buyer_and_seller_reporting_id)),
      selectable_buyer_and_seller_reporting_ids(
          std::move(selectable_buyer_and_seller_reporting_ids)),
      ad_render_id(std::move(ad_render_id)),
      allowed_reporting_origins(std::move(allowed_reporting_origins)) {
  if (render_gurl.is_valid()) {
    render_url_ = render_gurl.spec();
  }
}

InterestGroup::Ad::~Ad() = default;

size_t InterestGroup::Ad::EstimateSize() const {
  size_t size = 0u;
  size += render_url_.length();
  if (size_group) {
    size += size_group->size();
  }
  if (metadata) {
    size += metadata->size();
  }
  if (buyer_reporting_id) {
    size += buyer_reporting_id->size();
  }
  if (buyer_and_seller_reporting_id) {
    size += buyer_and_seller_reporting_id->size();
  }
  if (selectable_buyer_and_seller_reporting_ids) {
    for (auto& id : *selectable_buyer_and_seller_reporting_ids) {
      size += id.size();
    }
  }
  if (ad_render_id) {
    size += ad_render_id->size();
  }
  if (allowed_reporting_origins) {
    for (const url::Origin& origin : *allowed_reporting_origins) {
      size += origin.Serialize().size();
    }
  }
  return size;
}

bool InterestGroup::Ad::operator==(const Ad& other) const {
  return std::tie(render_url_, size_group, metadata, buyer_reporting_id,
                  buyer_and_seller_reporting_id,
                  selectable_buyer_and_seller_reporting_ids, ad_render_id,
                  allowed_reporting_origins) ==
         std::tie(other.render_url_, other.size_group, other.metadata,
                  other.buyer_reporting_id, other.buyer_and_seller_reporting_id,
                  other.selectable_buyer_and_seller_reporting_ids,
                  other.ad_render_id, other.allowed_reporting_origins);
}

InterestGroup::InterestGroup() = default;
InterestGroup::~InterestGroup() = default;
InterestGroup::InterestGroup(InterestGroup&& other) = default;
InterestGroup& InterestGroup::operator=(InterestGroup&& other) = default;
InterestGroup::InterestGroup(const InterestGroup& other) = default;
InterestGroup& InterestGroup::operator=(const InterestGroup& other) = default;

// The logic in this method must be kept in sync with ValidateBlinkInterestGroup
// in blink/renderer/modules/ad_auction/. The tests for this logic are also
// there, so they can be compared against each other.
bool InterestGroup::IsValid() const {
  if (owner.scheme() != url::kHttpsScheme) {
    return false;
  }

  if (!std::isfinite(priority)) {
    return false;
  }

  if (priority_vector) {
    for (const auto& [unused_signal_name, value] : *priority_vector) {
      if (!std::isfinite(value)) {
        return false;
      }
    }
  }

  if (priority_signals_overrides) {
    for (const auto& [unused_signal_name, value] :
         *priority_signals_overrides) {
      if (!std::isfinite(value)) {
        return false;
      }
    }
  }

  if (seller_capabilities) {
    for (const auto& [seller_origin, flags] : *seller_capabilities) {
      if (seller_origin.scheme() != url::kHttpsScheme) {
        return false;
      }
    }
  }

  if (execution_mode !=
          blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode &&
      execution_mode !=
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
      execution_mode !=
          blink::mojom::InterestGroup::ExecutionMode::kFrozenContext) {
    return false;
  }

  if (bidding_url && !IsUrlAllowed(*bidding_url, *this)) {
    return false;
  }

  if (bidding_wasm_helper_url &&
      !IsUrlAllowed(*bidding_wasm_helper_url, *this)) {
    return false;
  }

  if (update_url && !IsUrlAllowed(*update_url, *this)) {
    return false;
  }

  if (trusted_bidding_signals_url) {
    if (!IsUrlAllowedForTrustedBiddingSignals(*trusted_bidding_signals_url,
                                              *this)) {
      return false;
    }
  }

  if (trusted_bidding_signals_slot_size_mode !=
          blink::mojom::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kNone &&
      trusted_bidding_signals_slot_size_mode !=
          blink::mojom::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kSlotSize &&
      trusted_bidding_signals_slot_size_mode !=
          blink::mojom::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kAllSlotsRequestedSizes) {
    return false;
  }

  // `max_trusted_bidding_signals_url_length` must not be negative.
  if (max_trusted_bidding_signals_url_length < 0) {
    return false;
  }

  if (trusted_bidding_signals_coordinator) {
    if (trusted_bidding_signals_coordinator->scheme() != url::kHttpsScheme) {
      return false;
    }
  }

  if (ads) {
    for (const auto& ad : ads.value()) {
      if (!IsUrlAllowedForRenderUrls(GURL(ad.render_url()))) {
        return false;
      }
      if (ad.size_group) {
        if (ad.size_group->empty() || !size_groups ||
            !size_groups->contains(ad.size_group.value())) {
          return false;
        }
      }
      if (ad.ad_render_id) {
        if (ad.ad_render_id->size() > kMaxAdRenderIdSize) {
          return false;
        }
      }
      if (ad.allowed_reporting_origins) {
        if (ad.allowed_reporting_origins->size() >
            blink::mojom::kMaxAllowedReportingOrigins) {
          return false;
        }
        for (const auto& origin : ad.allowed_reporting_origins.value()) {
          if (origin.scheme() != url::kHttpsScheme) {
            return false;
          }
        }
      }
    }
  }

  if (ad_components) {
    for (const auto& ad : ad_components.value()) {
      if (!IsUrlAllowedForRenderUrls(GURL(ad.render_url()))) {
        return false;
      }
      if (ad.size_group) {
        if (ad.size_group->empty() || !size_groups ||
            !size_groups->contains(ad.size_group.value())) {
          return false;
        }
      }
      if (ad.ad_render_id) {
        if (ad.ad_render_id->size() > kMaxAdRenderIdSize) {
          return false;
        }
      }
      // These shouldn't be in components array.
      if (ad.buyer_reporting_id || ad.buyer_and_seller_reporting_id ||
          ad.selectable_buyer_and_seller_reporting_ids ||
          ad.allowed_reporting_origins) {
        return false;
      }
    }
  }

  if (ad_sizes) {
    for (auto const& [size_name, size_obj] : ad_sizes.value()) {
      if (size_name == "") {
        return false;
      }
      if (!IsValidAdSize(size_obj)) {
        return false;
      }
    }
  }

  if (size_groups) {
    // Sizes in a size group must also be in the ad_sizes map.
    if (!ad_sizes) {
      return false;
    }
    for (auto const& [group_name, size_list] : size_groups.value()) {
      if (group_name == "") {
        return false;
      }
      for (auto const& size_name : size_list) {
        if (size_name == "" || !ad_sizes->contains(size_name)) {
          return false;
        }
      }
    }
  }

  if (additional_bid_key) {
    if (additional_bid_key->size() != ED25519_PUBLIC_KEY_LEN) {
      return false;
    }
  }

  // InterestGroups used for negative targeting may not also have ads.
  // They are also not updatable.
  if (additional_bid_key && (ads || update_url)) {
    return false;
  }

  if (aggregation_coordinator_origin &&
      aggregation_coordinator_origin->scheme() != url::kHttpsScheme) {
    return false;
  }

  return EstimateSize() < blink::mojom::kMaxInterestGroupSize;
}

size_t InterestGroup::EstimateSize() const {
  size_t size = 0u;
  size += owner.Serialize().size();
  size += name.size();

  size += sizeof(priority);
  size += sizeof(execution_mode);
  size += sizeof(enable_bidding_signals_prioritization);

  if (priority_vector) {
    size += EstimateFlatMapSize(*priority_vector);
  }
  if (priority_signals_overrides) {
    size += EstimateFlatMapSize(*priority_signals_overrides);
  }
  if (seller_capabilities) {
    for (const auto& [seller_origin, flags] : *seller_capabilities) {
      size +=
          seller_origin.Serialize().size() + sizeof(decltype(flags)::EnumType);
    }
  }
  size += sizeof(decltype(all_sellers_capabilities)::EnumType);
  if (bidding_url) {
    size += bidding_url->spec().length();
  }
  if (bidding_wasm_helper_url) {
    size += bidding_wasm_helper_url->spec().length();
  }
  if (update_url) {
    size += update_url->spec().length();
  }
  if (trusted_bidding_signals_url) {
    size += trusted_bidding_signals_url->spec().length();
  }
  if (trusted_bidding_signals_keys) {
    for (const std::string& key : *trusted_bidding_signals_keys) {
      size += key.size();
    }
  }
  size += sizeof(trusted_bidding_signals_slot_size_mode);
  size += sizeof(max_trusted_bidding_signals_url_length);
  if (trusted_bidding_signals_coordinator) {
    size += trusted_bidding_signals_coordinator->Serialize().size();
  }
  if (user_bidding_signals) {
    size += user_bidding_signals->size();
  }
  if (ads) {
    for (const Ad& ad : *ads) {
      size += ad.EstimateSize();
    }
  }
  if (ad_components) {
    for (const Ad& ad : *ad_components) {
      size += ad.EstimateSize();
    }
  }
  if (ad_sizes) {
    for (const auto& [size_name, size_obj] : *ad_sizes) {
      size += size_name.length();
      size += sizeof(size_obj.width);
      size += sizeof(size_obj.height);
      size += sizeof(size_obj.width_units);
      size += sizeof(size_obj.height_units);
    }
  }
  if (size_groups) {
    for (const auto& size_group : size_groups.value()) {
      size += size_group.first.length();
      for (const auto& size_name : size_group.second) {
        size += size_name.length();
      }
    }
  }
  size += sizeof(decltype(auction_server_request_flags)::EnumType);
  if (additional_bid_key) {
    size += ED25519_PUBLIC_KEY_LEN;
  }
  if (aggregation_coordinator_origin) {
    size += aggregation_coordinator_origin->Serialize().size();
  }
  return size;
}

std::string_view InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
    TrustedBiddingSignalsSlotSizeMode slot_size_mode) {
  switch (slot_size_mode) {
    case TrustedBiddingSignalsSlotSizeMode::kNone:
      return "none";
    case TrustedBiddingSignalsSlotSizeMode::kSlotSize:
      return "slot-size";
    case TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes:
      return "all-slots-requested-sizes";
  }
}

std::string DEPRECATED_KAnonKeyForAdBid(
    const url::Origin& owner,
    const GURL& bidding_url,
    const std::string& ad_url_from_gurl_spec) {
  return base::StrCat({kKAnonKeyForAdBidPrefix, owner.GetURL().spec(), "\n",
                       bidding_url.spec(), "\n", ad_url_from_gurl_spec});
}

std::string DEPRECATED_KAnonKeyForAdBid(
    const InterestGroup& group,
    const std::string& ad_url_from_gurl_spec) {
  DCHECK(group.ads);
  DCHECK(base::Contains(
      *group.ads, ad_url_from_gurl_spec,
      [](const blink::InterestGroup::Ad& ad) { return ad.render_url(); }))
      << "No such ad: " << ad_url_from_gurl_spec;
  DCHECK(group.bidding_url);
  return DEPRECATED_KAnonKeyForAdBid(
      group.owner, group.bidding_url.value_or(GURL()), ad_url_from_gurl_spec);
}

std::string HashedKAnonKeyForAdBid(const url::Origin& owner,
                                   const GURL& bidding_url,
                                   const std::string& ad_url_from_gurl_spec) {
  return crypto::SHA256HashString(
      DEPRECATED_KAnonKeyForAdBid(owner, bidding_url, ad_url_from_gurl_spec));
}

std::string HashedKAnonKeyForAdBid(const InterestGroup& group,
                                   const std::string& ad_url_from_gurl_spec) {
  return crypto::SHA256HashString(
      DEPRECATED_KAnonKeyForAdBid(group, ad_url_from_gurl_spec));
}

std::string HashedKAnonKeyForAdBid(const InterestGroup& group,
                                   const blink::AdDescriptor& ad_descriptor) {
  return HashedKAnonKeyForAdBid(group, ad_descriptor.url.spec());
}

std::string DEPRECATED_KAnonKeyForAdComponentBid(
    const std::string& ad_url_from_gurl_spec) {
  return base::StrCat(
      {kKAnonKeyForAdComponentBidPrefix, ad_url_from_gurl_spec});
}

std::string HashedKAnonKeyForAdComponentBid(
    const std::string& ad_url_from_gurl_spec) {
  return crypto::SHA256HashString(
      DEPRECATED_KAnonKeyForAdComponentBid(ad_url_from_gurl_spec));
}

std::string HashedKAnonKeyForAdComponentBid(const GURL& ad_url) {
  return crypto::SHA256HashString(
      DEPRECATED_KAnonKeyForAdComponentBid(ad_url.spec()));
}

std::string HashedKAnonKeyForAdComponentBid(
    const blink::AdDescriptor& ad_descriptor) {
  // TODO(crbug.com/40266862): Add size back to this check.
  return HashedKAnonKeyForAdComponentBid(ad_descriptor.url.spec());
}
std::string DEPRECATED_KAnonKeyForAdNameReporting(
    const blink::InterestGroup& group,
    const blink::InterestGroup::Ad& ad,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id) {
  DCHECK(group.ads);
  DCHECK(base::Contains(*group.ads, ad)) << "No such ad: " << ad.render_url();
  DCHECK(group.bidding_url);
  return InternalPlainTextKAnonKeyForAdNameReporting(
      group.owner, group.name, group.bidding_url.value_or(GURL()),
      ad.render_url(), ad.buyer_reporting_id, ad.buyer_and_seller_reporting_id,
      selected_buyer_and_seller_reporting_id);
}

std::string HashedKAnonKeyForAdNameReporting(
    const blink::InterestGroup& group,
    const blink::InterestGroup::Ad& ad,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id) {
  return crypto::SHA256HashString(DEPRECATED_KAnonKeyForAdNameReporting(
      group, ad, selected_buyer_and_seller_reporting_id));
}

std::string HashedKAnonKeyForAdNameReportingWithoutInterestGroup(
    const url::Origin& interest_group_owner,
    const std::string& interest_group_name,
    const GURL& interest_group_bidding_url,
    const std::string& ad_render_url,
    base::optional_ref<const std::string> buyer_reporting_id,
    base::optional_ref<const std::string> buyer_and_seller_reporting_id,
    base::optional_ref<const std::string>
        selected_buyer_and_seller_reporting_id) {
  return crypto::SHA256HashString(InternalPlainTextKAnonKeyForAdNameReporting(
      interest_group_owner, interest_group_name, interest_group_bidding_url,
      ad_render_url, buyer_reporting_id, buyer_and_seller_reporting_id,
      selected_buyer_and_seller_reporting_id));
}
}  // namespace blink
