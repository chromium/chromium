// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/cookies/cookie_partition_key.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class CookiePartitionKeyTest : public testing::TestWithParam<bool> {
 protected:
  // testing::Test
  void SetUp() override {
    if (PartitionedCookiesEnabled())
      scoped_feature_list_.InitAndEnableFeature(features::kPartitionedCookies);
    testing::TestWithParam<bool>::SetUp();
  }

  bool PartitionedCookiesEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookiePartitionKeyTest,
                         testing::Bool());

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
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"))),
       true, "https://toplevelsite.com"},
      // Local file URL
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("file:///path/to/file.txt"))),
       true, "file://"},
      // File URL with host
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("file://toplevelsite.com/path/to/file.pdf"))),
       true, "file://toplevelsite.com"},
      // Opaque origin
      {absl::make_optional(CookiePartitionKey::FromURLForTesting(GURL())),
       false, ""},
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
       absl::make_optional(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com")))},
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
  EXPECT_FALSE(
      CookiePartitionKey::FromNetworkIsolationKey(NetworkIsolationKey()));

  SchemefulSite top_level_site =
      SchemefulSite(GURL("https://toplevelsite.com"));
  absl::optional<CookiePartitionKey> got =
      CookiePartitionKey::FromNetworkIsolationKey(NetworkIsolationKey(
          top_level_site, SchemefulSite(GURL("https://cookiesite.com"))));

  bool partitioned_cookies_enabled = PartitionedCookiesEnabled();
  EXPECT_EQ(partitioned_cookies_enabled, got.has_value());
  if (partitioned_cookies_enabled) {
    EXPECT_FALSE(got->from_script());
    EXPECT_EQ(CookiePartitionKey::FromURLForTesting(top_level_site.GetURL()),
              got.value());
  }
}

TEST_P(CookiePartitionKeyTest, FromNetworkIsolationKey_WithNonce) {
  const SchemefulSite kTopLevelSite =
      SchemefulSite(GURL("https://toplevelsite.com"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

  absl::optional<CookiePartitionKey> got =
      CookiePartitionKey::FromNetworkIsolationKey(NetworkIsolationKey(
          kTopLevelSite, SchemefulSite(GURL("https://cookiesite.com")),
          &kNonce));
  bool partitioned_cookies_enabled = PartitionedCookiesEnabled();
  EXPECT_EQ(partitioned_cookies_enabled, got.has_value());
  if (!partitioned_cookies_enabled)
    return;

  EXPECT_FALSE(got->from_script());
  EXPECT_TRUE(got->nonce());
  EXPECT_EQ(kTopLevelSite, got->site());
  EXPECT_EQ(kNonce, got->nonce().value());
}

TEST_P(CookiePartitionKeyTest, FromNetworkIsolationKey_WithFirstPartySetOwner) {
  SchemefulSite kTopLevelSite = SchemefulSite(GURL("https://setmember.com"));
  SchemefulSite kFirstPartySetOwnerSite =
      SchemefulSite(GURL("https://setowner.com"));
  SchemefulSite kCookieSite = SchemefulSite(GURL("https://cookiesite.com"));

  {
    // Without a nonce.
    absl::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            NetworkIsolationKey(kTopLevelSite, kCookieSite),
            &kFirstPartySetOwnerSite);
    bool partitioned_cookies_enabled = PartitionedCookiesEnabled();
    EXPECT_EQ(partitioned_cookies_enabled, got.has_value());
    if (partitioned_cookies_enabled) {
      EXPECT_FALSE(got->from_script());
      EXPECT_EQ(kFirstPartySetOwnerSite, got->site());
      EXPECT_FALSE(got->nonce());
    }
  }

  {
    // With a nonce.
    base::UnguessableToken nonce = base::UnguessableToken::Create();
    absl::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            NetworkIsolationKey(kTopLevelSite, kCookieSite, &nonce),
            &kFirstPartySetOwnerSite);
    bool partitioned_cookies_enabled = PartitionedCookiesEnabled();
    EXPECT_EQ(partitioned_cookies_enabled, got.has_value());
    if (partitioned_cookies_enabled) {
      EXPECT_FALSE(got->from_script());
      EXPECT_EQ(kFirstPartySetOwnerSite, got->site());
      EXPECT_TRUE(got->nonce());
      EXPECT_EQ(nonce, got->nonce().value());
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
  bool partitioned_cookies_enabled = PartitionedCookiesEnabled();
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
}

}  // namespace net
