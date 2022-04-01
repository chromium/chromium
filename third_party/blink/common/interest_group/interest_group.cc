// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group.h"

#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
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

InterestGroup::InterestGroup() : priority(0.0) {}

InterestGroup::InterestGroup(
    base::Time expiry,
    url::Origin owner,
    std::string name,
    double priority,
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

  if (!priority || !std::isfinite(*priority))
    return false;

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

  // priority is not stored as nullable, so only count the value size.
  size += sizeof(decltype(priority)::value_type);

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
  return std::tie(expiry, owner, name, priority, bidding_url,
                  bidding_wasm_helper_url, daily_update_url,
                  trusted_bidding_signals_url, trusted_bidding_signals_keys,
                  user_bidding_signals, ads, ad_components) ==
         std::tie(other.expiry, other.owner, other.name, other.priority,
                  other.bidding_url, other.bidding_wasm_helper_url,
                  other.daily_update_url, other.trusted_bidding_signals_url,
                  other.trusted_bidding_signals_keys,
                  other.user_bidding_signals, other.ads, other.ad_components);
}

}  // namespace blink
