// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/counted_cookie_access_details_set.h"

namespace network {

bool CookieAccessDetailsPrecede(const mojom::CookieAccessDetailsPtr& lhs,
                                const mojom::CookieAccessDetailsPtr& rhs) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/344653945): get these static asserts working for other
  // architectures / platforms.
  static_assert(
      sizeof(mojom::CookieAccessDetails) == 368,
      "CookieAccessDetailsPrecede needs to be updated if fields are added to "
      "CookieAccessDetails");
  static_assert(
      sizeof(mojom::SiteForCookies) == 88,
      "CookieAccessDetailsPrecede needs to be updated if fields are added to "
      "SiteForCookies");
#endif

  // We will check SiteForCookies fields first rather than including in the
  // tuple and tie below so that we can have a single section for LINTing.
  //
  // LINT.IfChange(SiteForCookies)
  if (lhs->site_for_cookies.site() < rhs->site_for_cookies.site()) {
    return true;
  }
  if (rhs->site_for_cookies.site() < lhs->site_for_cookies.site()) {
    return false;
  }
  if (lhs->site_for_cookies.schemefully_same() <
      rhs->site_for_cookies.schemefully_same()) {
    return true;
  }
  if (rhs->site_for_cookies.schemefully_same() <
      lhs->site_for_cookies.schemefully_same()) {
    return false;
  }
  // LINT.ThenChange(/services/network/public/mojom/site_for_cookies.mojom:SiteForCookies)

  // LINT.IfChange(CookieAccessDetails)
  const auto lhs_fields =
      std::make_tuple(std::tie(lhs->type, lhs->url, lhs->top_frame_origin,
                               lhs->devtools_request_id, lhs->is_ad_tagged),
                      lhs->cookie_setting_overrides.ToEnumBitmask());
  const auto rhs_fields =
      std::make_tuple(std::tie(rhs->type, rhs->url, rhs->top_frame_origin,
                               rhs->devtools_request_id, rhs->is_ad_tagged),
                      rhs->cookie_setting_overrides.ToEnumBitmask());
  if (lhs_fields < rhs_fields) {
    return true;
  }
  if (rhs_fields < lhs_fields) {
    return false;
  }
  return base::ranges::lexicographical_compare(
      lhs->cookie_list.begin(), lhs->cookie_list.end(),
      rhs->cookie_list.begin(), rhs->cookie_list.end(),
      [](const mojom::CookieOrLineWithAccessResultPtr& lhs_cookie,
         const mojom::CookieOrLineWithAccessResultPtr& rhs_cookie) {
        const auto lhs_pair = std::make_pair(
            lhs_cookie->access_result, lhs_cookie->cookie_or_line->which());
        const auto rhs_pair = std::make_pair(
            rhs_cookie->access_result, rhs_cookie->cookie_or_line->which());
        if (lhs_pair < rhs_pair) {
          return true;
        }
        if (rhs_pair < lhs_pair) {
          return false;
        }
        switch (lhs_cookie->cookie_or_line->which()) {
          case mojom::CookieOrLine::Tag::kCookie:
            return lhs_cookie->cookie_or_line->get_cookie().DataMembersPrecede(
                rhs_cookie->cookie_or_line->get_cookie());
          case mojom::CookieOrLine::Tag::kCookieString:
            return lhs_cookie->cookie_or_line->get_cookie_string() <
                   rhs_cookie->cookie_or_line->get_cookie_string();
        }
        NOTREACHED_NORETURN();
      });
  // LINT.ThenChange(/services/network/public/mojom/cookie_access_observer.mojom:CookieAccessDetails)
}

bool CookieAccessDetailsPtrComparer::operator()(
    const CountedCookieAccessDetailsPtr& lhs_counted,
    const CountedCookieAccessDetailsPtr& rhs_counted) const {
  return CookieAccessDetailsPrecede(lhs_counted.first, rhs_counted.first);
}
}  // namespace network
