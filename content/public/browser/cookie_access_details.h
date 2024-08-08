// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_ACCESS_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_ACCESS_DETAILS_H_

#include "content/common/content_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT CookieAccessDetails {
  using Type = network::mojom::CookieAccessDetails::Type;

  CookieAccessDetails();
  CookieAccessDetails(
      Type type,
      const GURL& url,
      const GURL& first_party_url,
      const net::CookieAccessResultList& cookie_access_result_list,
      bool blocked_by_policy = false,
      bool is_ad_tagged = false,
      const net::CookieSettingOverrides& cookie_setting_overrides =
          net::CookieSettingOverrides(),
      const net::SiteForCookies& site_for_cookies = net::SiteForCookies());
  ~CookieAccessDetails();

  CookieAccessDetails(const CookieAccessDetails&);
  CookieAccessDetails& operator=(const CookieAccessDetails&);

  Type type;
  GURL url;
  GURL first_party_url;
  net::CookieAccessResultList cookie_access_result_list;
  bool blocked_by_policy;
  bool is_ad_tagged = false;
  net::CookieSettingOverrides cookie_setting_overrides;
  // SiteForCookies is propagated to
  // PageSpecificContentSettings::OnCookiesAccessed which checks if cookies
  // that are same-site with the top-level frame but are accessed in a context
  // with a cross-site ancestor (aka ABA embeds) are blocked due to third-party
  // cookie blocking.
  net::SiteForCookies site_for_cookies;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_ACCESS_DETAILS_H_
