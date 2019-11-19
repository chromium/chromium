// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_TEST_UTILS_H_
#define CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_TEST_UTILS_H_

#include <string>
#include <vector>

#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {
class BrowserContext;

network::mojom::CookieManager* GetCookieManager(
    BrowserContext* browser_context);

void CreateCookieForTest(
    const std::string& cookie_name,
    const std::string& cookie_domain,
    net::CookieSameSite same_site,
    net::CookieOptions::SameSiteCookieContext cookie_context,
    bool is_cookie_secure,
    BrowserContext* browser_context);

std::vector<net::CanonicalCookie> GetAllCookies(
    BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_TEST_UTILS_H_
