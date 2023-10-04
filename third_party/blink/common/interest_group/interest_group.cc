// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include <stdint.h>

#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blink {

namespace {

const size_t kMaxAdRenderIdSize = 12;

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
    return false;

  return !url.has_username() && !url.has_password();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, update URL, or realtime data URL. Ad render URLs should be
// checked with IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check, and allows references.
bool IsUrlAllowed(const GURL& url, const InterestGroup& group) {
  if (url::Origin::Create(url) != group.owner)
    return false;

  return IsUrlAllowedForRenderUrls(url) && !url.has_ref();
}

size_t EstimateFlatMapSize(
    const base::flat_map<std::string, double>& flat_map) {
  size_t result = 0;
  for (const auto& pair : flat_map) {
    result += pair.first.length() + sizeof(pair.second);
  }
  return result;
}

}  // namespace

InterestGroup::Ad::Ad() = default;

InterestGroup::Ad::Ad(
    GURL render_url,
    absl::optional<std::string> metadata,
    absl::optional<std::string> size_group,
    absl::optional<std::string> buyer_reporting_id,
    absl::optional<std::string> buyer_and_seller_reporting_id,
    absl::optional<std::string> ad_render_id,
    absl::optional<std::vector<url::Origin>> allowed_reporting_origins)
    : render_url(std::move(render_url)),
      size_group(std::move(size_group)),
      metadata(std::move(metadata)),
      buyer_reporting_id(std::move(buyer_reporting_id)),
      buyer_and_seller_reporting_id(std::move(buyer_and_seller_reporting_id)),
      ad_render_id(std::move(ad_render_id)),
      allowed_reporting_origins(std::move(allowed_reporting_origins)) {}

InterestGroup::Ad::~Ad() = default;

size_t InterestGroup::Ad::EstimateSize() const {
  size_t size = 0u;
  size += render_url.spec().length();
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
  return std::tie(render_url, size_group, metadata, buyer_reporting_id,
                  buyer_and_seller_reporting_id, ad_render_id,
                  allowed_reporting_origins) ==
         std::tie(other.render_url, other.size_group, other.metadata,
                  other.buyer_reporting_id, other.buyer_and_seller_reporting_id,
                  other.ad_render_id, other.allowed_reporting_origins);
}

InterestGroup::InterestGroup() = default;
InterestGroup::~InterestGroup() = default;

// The logic in this method must be kept in sync with ValidateBlinkInterestGroup
// in blink/renderer/modules/ad_auction/. The tests for this logic are also
// there, so they can be compared against each other.
bool InterestGroup::IsValid() const {
  if (owner.scheme() != url::kHttpsScheme)
    return false;

  if (!std::isfinite(priority))
    return false;

  if (seller_capabilities) {
    for (const auto& [seller_origin, flags] : *seller_capabilities) {
      if (seller_origin.scheme() != url::kHttpsScheme)
        return false;
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

  if (bidding_url && !IsUrlAllowed(*bidding_url, *this))
    return false;

  if (bidding_wasm_helper_url &&
      !IsUrlAllowed(*bidding_wasm_helper_url, *this)) {
    return false;
  }

  if (update_url && !IsUrlAllowed(*update_url, *this)) {
    return false;
  }

  if (trusted_bidding_signals_url) {
    if (!IsUrlAllowed(*trusted_bidding_signals_url, *this))
      return false;

    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    if (trusted_bidding_signals_url->has_query())
      return false;
  }

  if (ads) {
    for (const auto& ad : ads.value()) {
      if (!IsUrlAllowedForRenderUrls(ad.render_url)) {
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
      if (!IsUrlAllowedForRenderUrls(ad.render_url)) {
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

  if (priority_vector)
    size += EstimateFlatMapSize(*priority_vector);
  if (priority_signals_overrides)
    size += EstimateFlatMapSize(*priority_signals_overrides);
  if (seller_capabilities) {
    for (const auto& [seller_origin, flags] : *seller_capabilities) {
      size +=
          seller_origin.Serialize().size() + sizeof(decltype(flags)::EnumType);
    }
  }
  size += sizeof(decltype(all_sellers_capabilities)::EnumType);
  if (bidding_url)
    size += bidding_url->spec().length();
  if (bidding_wasm_helper_url)
    size += bidding_wasm_helper_url->spec().length();
  if (update_url) {
    size += update_url->spec().length();
  }
  if (trusted_bidding_signals_url)
    size += trusted_bidding_signals_url->spec().length();
  if (trusted_bidding_signals_keys) {
    for (const std::string& key : *trusted_bidding_signals_keys)
      size += key.size();
  }
  if (user_bidding_signals)
    size += user_bidding_signals->size();
  if (ads) {
    for (const Ad& ad : *ads)
      size += ad.EstimateSize();
  }
  if (ad_components) {
    for (const Ad& ad : *ad_components)
      size += ad.EstimateSize();
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

bool InterestGroup::IsEqualForTesting(const InterestGroup& other) const {
  return std::tie(expiry, owner, name, priority,
                  enable_bidding_signals_prioritization, priority_vector,
                  priority_signals_overrides, seller_capabilities,
                  all_sellers_capabilities, execution_mode, bidding_url,
                  bidding_wasm_helper_url, update_url,
                  trusted_bidding_signals_url, trusted_bidding_signals_keys,
                  user_bidding_signals, ads, ad_components, ad_sizes,
                  size_groups, auction_server_request_flags, additional_bid_key,
                  aggregation_coordinator_origin) ==
         std::tie(
             other.expiry, other.owner, other.name, other.priority,
             other.enable_bidding_signals_prioritization, other.priority_vector,
             other.priority_signals_overrides, other.seller_capabilities,
             other.all_sellers_capabilities, other.execution_mode,
             other.bidding_url, other.bidding_wasm_helper_url, other.update_url,
             other.trusted_bidding_signals_url,
             other.trusted_bidding_signals_keys, other.user_bidding_signals,
             other.ads, other.ad_components, other.ad_sizes, other.size_groups,
             other.auction_server_request_flags, other.additional_bid_key,
             other.aggregation_coordinator_origin);
}

std::string KAnonKeyForAdBid(const InterestGroup& group, const GURL& ad_url) {
  return KAnonKeyForAdBid(group, blink::AdDescriptor(ad_url));
}

std::string KAnonKeyForAdBid(const blink::InterestGroup& group,
                             const blink::AdDescriptor& ad_descriptor) {
  DCHECK(group.ads);
  DCHECK(base::Contains(
      *group.ads, ad_descriptor.url,
      [](const blink::InterestGroup::Ad& ad) { return ad.render_url; }))
      << "No such ad: " << ad_descriptor.url;
  DCHECK(group.bidding_url);
  return KAnonKeyForAdBid(group.owner, group.bidding_url.value_or(GURL()),
                          ad_descriptor);
}

std::string KAnonKeyForAdBid(const url::Origin& owner,
                             const GURL& bidding_url,
                             const GURL& ad_url) {
  return KAnonKeyForAdBid(owner, bidding_url, blink::AdDescriptor(ad_url));
}

std::string KAnonKeyForAdBid(const url::Origin& owner,
                             const GURL& bidding_url,
                             const blink::AdDescriptor& ad_descriptor) {
  // TODO(crbug.com/1442242): Add size back to this check.
  return base::StrCat({kKAnonKeyForAdBidPrefix, owner.GetURL().spec(), "\n",
                       bidding_url.spec(), "\n", ad_descriptor.url.spec()});
}

std::string KAnonKeyForAdComponentBid(const GURL& ad_url) {
  return KAnonKeyForAdComponentBid(blink::AdDescriptor(ad_url));
}

std::string KAnonKeyForAdComponentBid(
    const blink::AdDescriptor& ad_descriptor) {
  // TODO(crbug.com/1442242): Add size back to this check.
  return base::StrCat(
      {kKAnonKeyForAdComponentBidPrefix, ad_descriptor.url.spec()});
}

std::string KAnonKeyForAdNameReporting(const blink::InterestGroup& group,
                                       const blink::InterestGroup::Ad& ad) {
  DCHECK(group.ads);
  DCHECK(base::Contains(*group.ads, ad)) << "No such ad: " << ad.render_url;
  DCHECK(group.bidding_url);
  std::string middle = base::StrCat({group.owner.GetURL().spec(), "\n",
                                     group.bidding_url.value_or(GURL()).spec(),
                                     "\n", ad.render_url.spec(), "\n"});
  if (ad.buyer_and_seller_reporting_id.has_value()) {
    return base::StrCat({kKAnonKeyForAdNameReportingBuyerAndSellerIdPrefix,
                         middle, *ad.buyer_and_seller_reporting_id});
  }
  if (ad.buyer_reporting_id.has_value()) {
    return base::StrCat({kKAnonKeyForAdNameReportingBuyerReportIdPrefix, middle,
                         *ad.buyer_reporting_id});
  }
  return base::StrCat(
      {kKAnonKeyForAdNameReportingNamePrefix, middle, group.name});
}

}  // namespace blink
