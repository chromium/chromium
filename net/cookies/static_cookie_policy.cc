// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/static_cookie_policy.h"

#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"

namespace net {

int StaticCookiePolicy::CanAccessCookies(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  switch (type_) {
    case StaticCookiePolicy::ALLOW_ALL_COOKIES:
      return OK;
    case StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES:
      return site_for_cookies.IsFirstParty(url) ? OK : ERR_ACCESS_DENIED;
    case StaticCookiePolicy::BLOCK_ALL_COOKIES:
      return ERR_ACCESS_DENIED;
    default:
      NOTREACHED_IN_MIGRATION();
      return ERR_ACCESS_DENIED;
  }
}

}  // namespace net
