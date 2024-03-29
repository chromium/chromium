// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cookies/cookie_inclusion_status.h"
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

TEST(CookieManagerSharedMojomTraitsTest,
     SerializeAndDeserializeCookiePartitionKey) {
  std::vector<net::CookiePartitionKey> keys = {
      net::CookiePartitionKey::FromURLForTesting(GURL("https://origin.com")),
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://origin.com"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()),
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://origin.com"),
          net::CookiePartitionKey::AncestorChainBit::kSameSite),
  };
  for (auto key : keys) {
    net::CookiePartitionKey copied =
        net::CookiePartitionKey::FromURLForTesting(GURL(""));
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::CookiePartitionKey>(
            key, copied));
    EXPECT_EQ(key, copied);
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
               WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE},
          net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial,
          /*use_literal=*/true);

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
  EXPECT_EQ(copied.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);
}

TEST(CookieManagerSharedMojomTraitsTest,
     Roundtrips_CookieInclusionStatus_ExemptionReason) {
  // This test case verifies exemption reason, otherwise it will be reset due to
  // exclusion reason.
  net::CookieInclusionStatus original =
      net::CookieInclusionStatus::MakeFromReasonsForTesting(
          {},
          {net::CookieInclusionStatus::
               WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT},
          net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial);

  net::CookieInclusionStatus copied;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              network::mojom::CookieInclusionStatus>(original, copied));
  EXPECT_TRUE(copied.IsInclude());
  EXPECT_TRUE(copied.HasExactlyWarningReasonsForTesting(
      {net::CookieInclusionStatus::
           WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));
  EXPECT_EQ(copied.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial);
}

}  // namespace mojo
