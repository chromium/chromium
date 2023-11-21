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
      const net::CookieList& list,
      size_t count,
      bool blocked_by_policy = false,
      bool is_ad_tagged = false,
      const net::CookieSettingOverrides& cookie_setting_overrides =
          net::CookieSettingOverrides());
  ~CookieAccessDetails();

  CookieAccessDetails(const CookieAccessDetails&);
  CookieAccessDetails& operator=(const CookieAccessDetails&);

  Type type;
  GURL url;
  GURL first_party_url;
  net::CookieList cookie_list;
  // CookieAccessDetails may be deduplicated to reduce IPC costs. In this case,
  // |count| refers to the number of instances that are duplicates of |this|
  // that would have been sent (including |this|).
  size_t count = 1u;
  bool blocked_by_policy;
  bool is_ad_tagged = false;
  net::CookieSettingOverrides cookie_setting_overrides;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_ACCESS_DETAILS_H_
