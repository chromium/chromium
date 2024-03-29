// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// It is not possible to create a `SameSite: None` cookie insecurely, so
// we can't explicitly test what happens when `secure = false` but the other
// parameters are correct.
struct AttributionCookieParams {
  const char* name;
  const char* domain;
  bool httponly;
  net::CookieSameSite same_site;
  std::optional<net::CookiePartitionKey> partition_key;
};

class AttributionCookieCheckerImplTest : public testing::Test {
 public:
  void SetCookie(const AttributionCookieParams& params) {
    auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
        /*name=*/params.name,
        /*value=*/"1",
        /*domain=*/params.domain,
        /*path=*/"/",
        /*creation=*/base::Time(),
        /*expiration=*/base::Time(),
        /*last_access=*/base::Time(),
        /*last_update=*/base::Time(),
        /*secure=*/true,
        /*httponly=*/params.httponly,
        /*same_site=*/params.same_site,
        /*priority=*/net::COOKIE_PRIORITY_DEFAULT,
        /*partition_key=*/params.partition_key);
    CHECK(cookie);

    net::CookieInclusionStatus result;
    base::RunLoop loop;

    browser_context_.GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(
            *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
            net::CookieOptions::MakeAllInclusive(),
            base::BindLambdaForTesting([&](net::CookieAccessResult r) {
              result = r.status;
              loop.Quit();
            }));
    loop.Run();

    ASSERT_TRUE(result.IsInclude()) << result.GetDebugString();
  }

  bool IsDebugCookieSet() {
    bool result;
    base::RunLoop loop;
    cookie_checker_.IsDebugCookieSet(
        url::Origin::Create(GURL("https://r.test")),
        base::BindLambdaForTesting([&](bool r) {
          result = r;
          loop.Quit();
        }));
    loop.Run();

    return result;
  }

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;

  AttributionCookieCheckerImpl cookie_checker_{
      static_cast<StoragePartitionImpl*>(
          browser_context_.GetDefaultStoragePartition())};
};

TEST_F(AttributionCookieCheckerImplTest, NoCookie_NotSet) {
  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, ValidCookie_Set) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r.test",
      .httponly = true,
      .same_site = net::CookieSameSite::NO_RESTRICTION,
  });

  EXPECT_TRUE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, WrongCookieName_NotSet) {
  SetCookie({
      .name = "AR_DEBUG",
      .domain = "r.test",
      .httponly = true,
      .same_site = net::CookieSameSite::NO_RESTRICTION,
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, WrongDomain_NotSet) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r2.test",
      .httponly = true,
      .same_site = net::CookieSameSite::NO_RESTRICTION,
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, NotHttpOnly_NotSet) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r.test",
      .httponly = false,
      .same_site = net::CookieSameSite::NO_RESTRICTION,
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, SameSiteLaxMode_NotSet) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r.test",
      .httponly = false,
      .same_site = net::CookieSameSite::LAX_MODE,
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, SameSiteStrictMode_NotSet) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r.test",
      .httponly = false,
      .same_site = net::CookieSameSite::STRICT_MODE,
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

TEST_F(AttributionCookieCheckerImplTest, Partitioned_NotSet) {
  SetCookie({
      .name = "ar_debug",
      .domain = "r.test",
      .httponly = false,
      .same_site = net::CookieSameSite::NO_RESTRICTION,
      // Mojo deserialization crashes without the unguessable token here.
      .partition_key = net::CookiePartitionKey::FromURLForTesting(
          GURL("https://r2.test"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()),
  });

  EXPECT_FALSE(IsDebugCookieSet());
}

}  // namespace
}  // namespace content
