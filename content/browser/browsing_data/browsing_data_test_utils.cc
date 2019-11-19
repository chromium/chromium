// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_test_utils.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using CookieInclusionStatus = net::CanonicalCookie::CookieInclusionStatus;

namespace content {

network::mojom::CookieManager* GetCookieManager(
    BrowserContext* browser_context) {
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  return storage_partition->GetCookieManagerForBrowserProcess();
}

void CreateCookieForTest(
    const std::string& cookie_name,
    const std::string& cookie_domain,
    net::CookieSameSite same_site,
    net::CookieOptions::SameSiteCookieContext cookie_context,
    bool is_cookie_secure,
    BrowserContext* browser_context) {
  base::RunLoop run_loop;
  net::CookieOptions options;
  options.set_same_site_cookie_context(cookie_context);
  bool result_out;
  GetCookieManager(browser_context)
      ->SetCanonicalCookie(
          net::CanonicalCookie(cookie_name, "1", cookie_domain, "/",
                               base::Time(), base::Time(), base::Time(),
                               is_cookie_secure, false, same_site,
                               net::COOKIE_PRIORITY_LOW),
          "https", options,
          base::BindLambdaForTesting([&](CookieInclusionStatus result) {
            result_out = result.IsInclude();
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(result_out);
}

std::vector<net::CanonicalCookie> GetAllCookies(
    BrowserContext* browser_context) {
  base::RunLoop run_loop;
  std::vector<net::CanonicalCookie> cookies_out;
  GetCookieManager(browser_context)
      ->GetAllCookies(base::BindLambdaForTesting(
          [&](const std::vector<net::CanonicalCookie>& cookies) {
            cookies_out = cookies;
            run_loop.Quit();
          }));
  run_loop.Run();
  return cookies_out;
}

}  // namespace content
