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

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blink {

namespace {

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

InterestGroup::Ad::Ad(GURL render_url, absl::optional<std::string> metadata)
    : render_url(std::move(render_url)), metadata(std::move(metadata)) {}

InterestGroup::Ad::~Ad() = default;

size_t InterestGroup::Ad::EstimateSize() const {
  size_t size = 0u;
  size += render_url.spec().length();
  if (metadata)
    size += metadata->size();
  return size;
}

bool InterestGroup::Ad::operator==(const Ad& other) const {
  return render_url == other.render_url && metadata == other.metadata;
}

InterestGroup::InterestGroup() = default;

InterestGroup::InterestGroup(
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
    blink::mojom::InterestGroup::ExecutionMode execution_mode,
    absl::optional<GURL> bidding_url,
    absl::optional<GURL> bidding_wasm_helper_url,
    absl::optional<GURL> daily_update_url,
    absl::optional<GURL> trusted_bidding_signals_url,
    absl::optional<std::vector<std::string>> trusted_bidding_signals_keys,
    absl::optional<std::string> user_bidding_signals,
    absl::optional<std::vector<InterestGroup::Ad>> ads,
    absl::optional<std::vector<InterestGroup::Ad>> ad_components)
    : expiry(expiry),
      owner(std::move(owner)),
      name(std::move(name)),
      priority(priority),
      enable_bidding_signals_prioritization(
          enable_bidding_signals_prioritization),
      priority_vector(std::move(priority_vector)),
      priority_signals_overrides(std::move(priority_signals_overrides)),
      seller_capabilities(std::move(seller_capabilities)),
      all_sellers_capabilities(all_sellers_capabilities),
      execution_mode(execution_mode),
      bidding_url(std::move(bidding_url)),
      bidding_wasm_helper_url(std::move(bidding_wasm_helper_url)),
      daily_update_url(std::move(daily_update_url)),
      trusted_bidding_signals_url(std::move(trusted_bidding_signals_url)),
      trusted_bidding_signals_keys(std::move(trusted_bidding_signals_keys)),
      user_bidding_signals(std::move(user_bidding_signals)),
      ads(std::move(ads)),
      ad_components(std::move(ad_components)) {}

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
          blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
    return false;
  }

  if (bidding_url && !IsUrlAllowed(*bidding_url, *this))
    return false;

  if (bidding_wasm_helper_url &&
      !IsUrlAllowed(*bidding_wasm_helper_url, *this)) {
    return false;
  }

  if (daily_update_url && !IsUrlAllowed(*daily_update_url, *this))
    return false;

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
      if (!IsUrlAllowedForRenderUrls(ad.render_url))
        return false;
    }
  }

  if (ad_components) {
    for (const auto& ad : ad_components.value()) {
      if (!IsUrlAllowedForRenderUrls(ad.render_url))
        return false;
    }
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
  if (daily_update_url)
    size += daily_update_url->spec().length();
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
  return size;
}

bool InterestGroup::IsEqualForTesting(const InterestGroup& other) const {
  return std::tie(expiry, owner, name, priority,
                  enable_bidding_signals_prioritization, priority_vector,
                  priority_signals_overrides, seller_capabilities,
                  all_sellers_capabilities, execution_mode, bidding_url,
                  bidding_wasm_helper_url, daily_update_url,
                  trusted_bidding_signals_url, trusted_bidding_signals_keys,
                  user_bidding_signals, ads, ad_components) ==
         std::tie(other.expiry, other.owner, other.name, other.priority,
                  other.enable_bidding_signals_prioritization,
                  other.priority_vector, other.priority_signals_overrides,
                  other.seller_capabilities, other.all_sellers_capabilities,
                  other.execution_mode, other.bidding_url,
                  other.bidding_wasm_helper_url, other.daily_update_url,
                  other.trusted_bidding_signals_url,
                  other.trusted_bidding_signals_keys,
                  other.user_bidding_signals, other.ads, other.ad_components);
}

}  // namespace blink
