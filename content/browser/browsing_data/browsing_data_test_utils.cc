// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_test_utils.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

network::mojom::CookieManager* GetCookieManager(
    BrowserContext* browser_context) {
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
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
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      cookie_name, "1", cookie_domain, "/", base::Time(), base::Time(),
      base::Time(), base::Time(), is_cookie_secure, false, same_site,
      net::COOKIE_PRIORITY_LOW);
  GetCookieManager(browser_context)
      ->SetCanonicalCookie(
          *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
          options,
          base::BindLambdaForTesting([&](net::CookieAccessResult result) {
            result_out = result.status.IsInclude();
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
