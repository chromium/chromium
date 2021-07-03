// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/validate_interest_group.h"

#include <vector>

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
bool IsUrlAllowed(const GURL& url, const mojom::InterestGroup& group) {
  if (url::Origin::Create(url) != group.owner)
    return false;

  return IsUrlAllowedForRenderUrls(url) && !url.has_ref();
}

}  // namespace

// The logic in this method must be kept in sync with ValidateBlinkInterestGroup
// in blink/renderer/modules/ad_auction/. The tests for this logic are also
// there, so they can be compared against each other.
bool ValidateInterestGroup(const url::Origin& origin,
                           const mojom::InterestGroup& group) {
  if (origin.scheme() != url::kHttpsScheme)
    return false;

  if (group.owner != origin)
    return false;

  if (group.bidding_url && !IsUrlAllowed(*group.bidding_url, group))
    return false;

  if (group.update_url && !IsUrlAllowed(*group.update_url, group))
    return false;

  if (group.trusted_bidding_signals_url) {
    if (!IsUrlAllowed(*group.trusted_bidding_signals_url, group))
      return false;

    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    if (group.trusted_bidding_signals_url->has_query())
      return false;
  }

  if (group.ads) {
    for (const auto& ad : group.ads.value()) {
      if (!IsUrlAllowedForRenderUrls(ad->render_url))
        return false;
    }
  }

  return true;
}

}  // namespace blink
