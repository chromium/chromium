// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class CookiePartitionKeyTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  // testing::Test
  void SetUp() override {
    scoped_feature_list_[0].InitWithFeatureState(features::kPartitionedCookies,
                                                 PartitionedCookiesEnabled());
    scoped_feature_list_[1].InitWithFeatureState(
        features::kNoncedPartitionedCookies, NoncedPartitionedCookiesEnabled());
  }

  bool PartitionedCookiesEnabled() { return std::get<0>(GetParam()); }
  bool NoncedPartitionedCookiesEnabled() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_[2];
};

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookiePartitionKeyTest,
                         ::testing::Values(std::make_tuple(false, false),
                                           std::make_tuple(false, true),
                                           std::make_tuple(true, true)));

TEST_P(CookiePartitionKeyTest, Serialization) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  struct {
    absl::optional<CookiePartitionKey> input;
    bool expected_ret;
    std::string expected_output;
  } cases[] = {
      // No partition key
      {absl::nullopt, true, kEmptyCookiePartitionKey},
      // Partition key present
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com")),
       true, "https://toplevelsite.com"},
      // Local file URL
      {CookiePartitionKey::FromURLForTesting(GURL("file:///path/to/file.txt")),
       true, "file://"},
      // File URL with host
      {CookiePartitionKey::FromURLForTesting(
           GURL("file://toplevelsite.com/path/to/file.pdf")),
       true, "file://toplevelsite.com"},
      // Opaque origin
      {CookiePartitionKey::FromURLForTesting(GURL()), false, ""},
      // With nonce
      {CookiePartitionKey::FromNetworkIsolationKey(NetworkIsolationKey(
           SchemefulSite(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://cookiesite.com")), &nonce)),
       false, ""},
      // Invalid partition key
      {absl::make_optional(
           CookiePartitionKey::FromURLForTesting(GURL("abc123foobar!!"))),
       false, ""},
  };

  for (const auto& tc : cases) {
    std::string got;
    if (PartitionedCookiesEnabled()) {
      EXPECT_EQ(tc.expected_ret, CookiePartitionKey::Serialize(tc.input, got));
      EXPECT_EQ(tc.expected_output, got);
    } else {
      // Serialize should only return true for unpartitioned cookies if the
      // feature is disabled.
      EXPECT_NE(tc.input.has_value(),
                CookiePartitionKey::Serialize(tc.input, got));
    }
  }
}

TEST_P(CookiePartitionKeyTest, Deserialization) {
  struct {
    std::string input;
    bool expected_ret;
    absl::optional<CookiePartitionKey> expected_output;
  } cases[] = {
      {kEmptyCookiePartitionKey, true, absl::nullopt},
      {"https://toplevelsite.com", true,
       CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"))},
      {"abc123foobar!!", false, absl::nullopt},
  };

  for (const auto& tc : cases) {
    absl::optional<CookiePartitionKey> got;
    if (PartitionedCookiesEnabled()) {
      EXPECT_EQ(tc.expected_ret,
                CookiePartitionKey::Deserialize(tc.input, got));
      if (tc.expected_output.has_value()) {
        EXPECT_TRUE(got.has_value());
        EXPECT_EQ(tc.expected_output.value(), got.value());
      } else {
        EXPECT_FALSE(got.has_value());
      }
    } else {
      // Deserialize should only return true for unpartitioned cookies if the
      // feature is disabled.
      EXPECT_EQ(tc.input == kEmptyCookiePartitionKey,
                CookiePartitionKey::Deserialize(tc.input, got));
    }
  }
}

TEST_P(CookiePartitionKeyTest, FromNetworkIsolationKey) {
  const SchemefulSite kTopLevelSite =
      SchemefulSite(GURL("https://toplevelsite.com"));
  const SchemefulSite kCookieSite =
      SchemefulSite(GURL("https://cookiesite.com"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

  struct TestCase {
    const std::string desc;
    const NetworkIsolationKey network_isolation_key;
    bool allow_nonced_partition_keys;
    const absl::optional<CookiePartitionKey> expected;
  } test_cases[] = {
      {
          "Empty",
          NetworkIsolationKey(),
          /*allow_nonced_partition_keys=*/false,
          absl::nullopt,
      },
      {
          "WithTopLevelSite",
          NetworkIsolationKey(kTopLevelSite, kCookieSite),
          /*allow_nonced_partition_keys=*/false,
          CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
      },
      {
          "WithNonce",
          NetworkIsolationKey(kTopLevelSite, kCookieSite, &kNonce),
          /*allow_nonced_partition_keys=*/false,
          CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kNonce),
      },
      {
          "NoncedAllowed_KeyWithoutNonce",
          NetworkIsolationKey(kTopLevelSite, kCookieSite),
          /*allow_nonced_partition_keys=*/true,
          CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
      },
      {
          "NoncedAllowed_KeyWithoutNonce",
          NetworkIsolationKey(kTopLevelSite, kCookieSite, &kNonce),
          /*allow_nonced_partition_keys=*/true,
          CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kNonce),
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);

    base::test::ScopedFeatureList feature_list;
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (PartitionedCookiesEnabled()) {
      enabled_features.push_back(features::kPartitionedCookies);
    } else {
      disabled_features.push_back(features::kPartitionedCookies);
    }
    if (test_case.allow_nonced_partition_keys) {
      enabled_features.push_back(features::kNoncedPartitionedCookies);
    } else {
      disabled_features.push_back(features::kNoncedPartitionedCookies);
    }
    feature_list.InitWithFeatures(enabled_features, disabled_features);

    absl::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            test_case.network_isolation_key);

    if (got)
      EXPECT_FALSE(got->from_script());

    if (PartitionedCookiesEnabled()) {
      EXPECT_EQ(test_case.expected, got);
    } else if (test_case.allow_nonced_partition_keys) {
      EXPECT_EQ(test_case.network_isolation_key.GetNonce().has_value(),
                got.has_value());
      if (got)
        EXPECT_EQ(test_case.expected, got);
    } else {
      EXPECT_FALSE(got);
    }
  }
}

TEST_P(CookiePartitionKeyTest, FromWire) {
  struct TestCase {
    const GURL url;
    const absl::optional<base::UnguessableToken> nonce;
  } test_cases[] = {
      {GURL("https://foo.com"), absl::nullopt},
      {GURL(), absl::nullopt},
      {GURL("https://foo.com"),
       absl::make_optional(base::UnguessableToken::Create())},
  };

  for (const auto& test_case : test_cases) {
    auto want =
        CookiePartitionKey::FromURLForTesting(test_case.url, test_case.nonce);
    auto got = CookiePartitionKey::FromWire(want.site(), want.nonce());
    EXPECT_EQ(want, got);
    EXPECT_FALSE(got.from_script());
  }
}

TEST_P(CookiePartitionKeyTest, FromScript) {
  auto key = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key);
  EXPECT_TRUE(key->from_script());
  EXPECT_TRUE(key->site().opaque());
}

TEST_P(CookiePartitionKeyTest, IsSerializeable) {
  EXPECT_FALSE(CookiePartitionKey::FromURLForTesting(GURL()).IsSerializeable());
  EXPECT_EQ(PartitionedCookiesEnabled(), CookiePartitionKey::FromURLForTesting(
                                             GURL("https://www.example.com"))
                                             .IsSerializeable());
}

TEST_P(CookiePartitionKeyTest, Equality_WithNonce) {
  SchemefulSite top_level_site =
      SchemefulSite(GURL("https://toplevelsite.com"));
  SchemefulSite frame_site = SchemefulSite(GURL("https://cookiesite.com"));
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  EXPECT_NE(nonce1, nonce2);
  auto key1 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, &nonce1));
  bool partitioned_cookies_enabled =
      PartitionedCookiesEnabled() || NoncedPartitionedCookiesEnabled();
  EXPECT_EQ(partitioned_cookies_enabled, key1.has_value());
  if (!partitioned_cookies_enabled)
    return;

  auto key2 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, &nonce2));
  EXPECT_TRUE(key1.has_value() && key2.has_value());
  EXPECT_NE(key1, key2);

  auto key3 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, &nonce1));
  EXPECT_EQ(key1, key3);

  auto unnonced_key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site));
  EXPECT_NE(key1, unnonced_key);
}

}  // namespace net
