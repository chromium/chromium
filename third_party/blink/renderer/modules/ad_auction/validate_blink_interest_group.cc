// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/url_constants.h"

namespace blink {

namespace {

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const KURL& url) {
  if (!url.IsValid() || !url.ProtocolIs(url::kHttpsScheme))
    return false;

  return url.User().IsEmpty() && url.Pass().IsEmpty();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, update URL, or realtime data URL. Ad render URLs should be
// checked with IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check, and allows references.
bool IsUrlAllowed(const KURL& url, const mojom::blink::InterestGroup& group) {
  if (!group.owner->IsSameOriginWith(SecurityOrigin::Create(url).get()))
    return false;

  return IsUrlAllowedForRenderUrls(url) && !url.HasFragmentIdentifier();
}

}  // namespace

// The logic in this method must be kept in sync with InterestGroup::IsValid()
// in blink/common/interest_group/.
bool ValidateBlinkInterestGroup(const mojom::blink::InterestGroup& group,
                                String& error_field_name,
                                String& error_field_value,
                                String& error) {
  if (group.owner->Protocol() != url::kHttpsScheme) {
    error_field_name = String::FromUTF8("owner");
    error_field_value = group.owner->ToString();
    error = String::FromUTF8("owner origin must be HTTPS.");
    return false;
  }

  if (group.bidding_url && !IsUrlAllowed(*group.bidding_url, group)) {
    error_field_name = String::FromUTF8("biddingUrl");
    error_field_value = group.bidding_url->GetString();
    error = String::FromUTF8(
        "biddingUrl must have the same origin as the InterestGroup owner "
        "and have no fragment identifier or embedded credentials.");
    return false;
  }

  if (group.update_url && !IsUrlAllowed(*group.update_url, group)) {
    error_field_name = String::FromUTF8("updateUrl");
    error_field_value = group.update_url->GetString();
    error = String::FromUTF8(
        "updateUrl must have the same origin as the InterestGroup owner "
        "and have no fragment identifier or embedded credentials.");
    return false;
  }

  if (group.trusted_bidding_signals_url) {
    // In addition to passing the same checks used on the other URLs,
    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    if (!IsUrlAllowed(*group.trusted_bidding_signals_url, group) ||
        !group.trusted_bidding_signals_url->Query().IsEmpty()) {
      error_field_name = String::FromUTF8("trustedBiddingSignalsUrl");
      error_field_value = group.trusted_bidding_signals_url->GetString();
      error = String::FromUTF8(
          "trustedBiddingSignalsUrl must have the same origin as the "
          "InterestGroup owner and have no query string, fragment identifier "
          "or embedded credentials.");
      return false;
    }
  }

  if (group.ads) {
    for (WTF::wtf_size_t i = 0; i < group.ads.value().size(); ++i) {
      const KURL& render_url = group.ads.value()[i]->render_url;
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("ad[%u].renderUrl", i);
        error_field_value = render_url.GetString();
        error = String::FromUTF8(
            "renderUrls must be HTTPS and have no embedded credentials.");
        return false;
      }
    }
  }

  if (group.ad_components) {
    for (WTF::wtf_size_t i = 0; i < group.ad_components.value().size(); ++i) {
      const KURL& render_url = group.ad_components.value()[i]->render_url;
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("adComponent[%u].renderUrl", i);
        error_field_value = render_url.GetString();
        error = String::FromUTF8(
            "renderUrls must be HTTPS and have no embedded credentials.");
        return false;
      }
    }
  }

  return true;
}

}  // namespace blink
