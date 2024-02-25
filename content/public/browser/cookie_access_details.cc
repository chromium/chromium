// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cookie_access_details.h"

namespace content {

CookieAccessDetails::CookieAccessDetails() = default;
CookieAccessDetails::~CookieAccessDetails() = default;

CookieAccessDetails::CookieAccessDetails(
    Type type,
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    size_t count,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides)
    : type(type),
      url(url),
      first_party_url(first_party_url),
      cookie_list(cookie_list),
      count(count),
      blocked_by_policy(blocked_by_policy),
      is_ad_tagged(is_ad_tagged),
      cookie_setting_overrides(cookie_setting_overrides) {}

CookieAccessDetails::CookieAccessDetails(const CookieAccessDetails& details) =
    default;
CookieAccessDetails& CookieAccessDetails::operator=(
    const CookieAccessDetails& details) = default;

}  // namespace content
