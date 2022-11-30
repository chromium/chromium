// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/site_for_cookies.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {

TEST(CookieManagerSharedMojomTraitsTest, SerializeAndDeserialize) {
  std::vector<net::SiteForCookies> keys = {
      net::SiteForCookies(),
      net::SiteForCookies::FromUrl(GURL("file:///whatver")),
      net::SiteForCookies::FromUrl(GURL("ws://127.0.0.1/things")),
      net::SiteForCookies::FromUrl(GURL("https://example.com"))};

  for (auto original : keys) {
    net::SiteForCookies copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::SiteForCookies>(
            original, copied));
    EXPECT_TRUE(original.IsEquivalent(copied));
    EXPECT_EQ(original.schemefully_same(), copied.schemefully_same());
  }
}

TEST(CookieManagerSharedMojomTraitsTest, Roundtrips_CookieInclusionStatus) {
  // This status + warning combo doesn't really make sense. It's just an
  // arbitrary selection of values to test the serialization/deserialization.
  net::CookieInclusionStatus original =
      net::CookieInclusionStatus::MakeFromReasonsForTesting(
          {net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
           net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
           net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY},
          {net::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
           net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE,
           net::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE});

  net::CookieInclusionStatus copied;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::CookieInclusionStatus>(original, copied));
  EXPECT_TRUE(copied.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
       net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX,
       net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
  EXPECT_TRUE(copied.HasExactlyWarningReasonsForTesting(
      {net::CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
       net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE,
       net::CookieInclusionStatus::
           WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE}));
}

}  // namespace mojo
