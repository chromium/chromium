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

TEST(CookiePartitionKeyTest, TestFromStorage) {
  struct {
    const std::string top_level_site;
    bool expected_return;
    const std::optional<CookiePartitionKey> expected_output;
  } cases[] = {{/*empty site*/
                "", true, std::nullopt},
               /*invalid site*/
               {"Invalid", false, std::nullopt},
               /*valid site*/
               {"https://toplevelsite.com", true,
                CookiePartitionKey::FromURLForTesting(
                    GURL("https://toplevelsite.com"))}};

  for (const auto& tc : cases) {
    base::expected<std::optional<CookiePartitionKey>, std::string> got =
        CookiePartitionKey::FromStorage(tc.top_level_site);
    EXPECT_EQ(got.has_value(), tc.expected_return);
    if (got.has_value()) {
      EXPECT_EQ(got.value(), tc.expected_output);
    }
  }
}

TEST(CookiePartitionKeyTest, TestFromUntrustedInput) {
  const std::string kFullURL = "https://subdomain.toplevelsite.com/index.html";
  const std::string kValidSite = "https://toplevelsite.com";
  struct {
    const std::string top_level_site;
    bool partition_key_created;
  } cases[] = {{/*empty site*/
                "", false},
               {/*valid site*/
                kValidSite, true},
               {/*full url*/
                kFullURL, true},
               {/*invalid site (missing scheme)*/
                "toplevelsite.com", false},
               {/*invalid site*/
                "abc123foobar!!", false}};

  for (const auto& tc : cases) {
    base::expected<CookiePartitionKey, std::string> got =
        CookiePartitionKey::FromUntrustedInput(tc.top_level_site);
    EXPECT_EQ(got.has_value(), tc.partition_key_created);
    if (tc.partition_key_created) {
      EXPECT_EQ(got->site().Serialize(), kValidSite);
    }
  }
}

TEST(CookiePartitionKeyTest, Serialization) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  struct {
    const std::optional<CookiePartitionKey> input;
    bool expected_success;
    const std::string expected_output_top_level_site;
  } cases[] = {
      // No partition key
      {std::nullopt, true, kEmptyCookiePartitionKey},
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
           SchemefulSite(GURL("https://cookiesite.com")), nonce)),
       false, ""},
      // Invalid partition key
      {CookiePartitionKey::FromURLForTesting(GURL("abc123foobar!!")), false,
       ""},
  };

  for (const auto& tc : cases) {
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        got = CookiePartitionKey::Serialize(tc.input);

    EXPECT_EQ(tc.expected_success, got.has_value());
    if (got.has_value()) {
      // TODO (crbug.com/41486025) once ancestor chain bit is implemented update
      // test to check bit's value.
      EXPECT_EQ(tc.expected_output_top_level_site, got->TopLevelSite());
    }
  }
}

TEST(CookiePartitionKeyTest, FromNetworkIsolationKey) {
  const SchemefulSite kTopLevelSite =
      SchemefulSite(GURL("https://toplevelsite.com"));
  const SchemefulSite kCookieSite =
      SchemefulSite(GURL("https://cookiesite.com"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

  struct TestCase {
    const std::string desc;
    const NetworkIsolationKey network_isolation_key;
    const std::optional<CookiePartitionKey> expected;
  } test_cases[] = {
      {
          "Empty",
          NetworkIsolationKey(),
          std::nullopt,
      },
      {
          "WithTopLevelSite",
          NetworkIsolationKey(kTopLevelSite, kCookieSite),
          CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
      },
      {
          "WithNonce",
          NetworkIsolationKey(kTopLevelSite, kCookieSite, kNonce),
          CookiePartitionKey::FromURLForTesting(kCookieSite.GetURL(), kNonce),
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);

    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            test_case.network_isolation_key);

    EXPECT_EQ(test_case.expected, got);
    if (got) {
      EXPECT_EQ(test_case.network_isolation_key.GetNonce(), got->nonce());
    }
  }
}

TEST(CookiePartitionKeyTest, FromWire) {
  struct TestCase {
    const GURL url;
    const std::optional<base::UnguessableToken> nonce;
  } test_cases[] = {
      {GURL("https://foo.com"), std::nullopt},
      {GURL(), std::nullopt},
      {GURL("https://foo.com"), base::UnguessableToken::Create()},
  };

  for (const auto& test_case : test_cases) {
    auto want =
        CookiePartitionKey::FromURLForTesting(test_case.url, test_case.nonce);
    auto got = CookiePartitionKey::FromWire(want.site(), want.nonce());
    EXPECT_EQ(want, got);
    EXPECT_FALSE(got.from_script());
  }
}

TEST(CookiePartitionKeyTest, FromStorageKeyComponents) {
  struct TestCase {
    const GURL url;
    const std::optional<base::UnguessableToken> nonce = std::nullopt;
  } test_cases[] = {
      {GURL("https://foo.com")},
      {GURL()},
      {GURL("https://foo.com"), base::UnguessableToken::Create()},
  };

  for (const auto& test_case : test_cases) {
    auto want =
        CookiePartitionKey::FromURLForTesting(test_case.url, test_case.nonce);
    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromStorageKeyComponents(want.site(), want.nonce());
    EXPECT_EQ(got, want);
  }
}

TEST(CookiePartitionKeyTest, FromScript) {
  auto key = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key);
  EXPECT_TRUE(key->from_script());
  EXPECT_TRUE(key->site().opaque());

  auto key2 = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key2);
  EXPECT_TRUE(key2->from_script());
  EXPECT_TRUE(key2->site().opaque());

  // The keys should not be equal because they get created with different opaque
  // sites. Test both the '==' and '!=' operators here.
  EXPECT_FALSE(key == key2);
  EXPECT_TRUE(key != key2);
}

TEST(CookiePartitionKeyTest, IsSerializeable) {
  EXPECT_FALSE(CookiePartitionKey::FromURLForTesting(GURL()).IsSerializeable());
  EXPECT_TRUE(
      CookiePartitionKey::FromURLForTesting(GURL("https://www.example.com"))
          .IsSerializeable());
}

TEST(CookiePartitionKeyTest, Equality) {
  // Same eTLD+1 but different scheme are not equal.
  EXPECT_NE(CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")),
            CookiePartitionKey::FromURLForTesting(GURL("http://foo.com")));

  // Different subdomains of the same site are equal.
  EXPECT_EQ(CookiePartitionKey::FromURLForTesting(GURL("https://a.foo.com")),
            CookiePartitionKey::FromURLForTesting(GURL("https://b.foo.com")));
}

TEST(CookiePartitionKeyTest, Equality_WithNonce) {
  SchemefulSite top_level_site =
      SchemefulSite(GURL("https://toplevelsite.com"));
  SchemefulSite frame_site = SchemefulSite(GURL("https://cookiesite.com"));
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  EXPECT_NE(nonce1, nonce2);
  auto key1 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1));
  EXPECT_TRUE(key1.has_value());

  auto key2 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce2));
  EXPECT_TRUE(key1.has_value() && key2.has_value());
  EXPECT_NE(key1, key2);

  auto key3 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1));
  EXPECT_EQ(key1, key3);

  auto unnonced_key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site));
  EXPECT_NE(key1, unnonced_key);
}

TEST(CookiePartitionKeyTest, Localhost) {
  SchemefulSite top_level_site(GURL("https://localhost:8000"));

  auto key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, top_level_site));
  EXPECT_TRUE(key.has_value());

  SchemefulSite frame_site(GURL("https://cookiesite.com"));
  key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site));
  EXPECT_TRUE(key.has_value());
}

// Test that creating nonced partition keys works with both types of
// NetworkIsolationKey modes. See https://crbug.com/1442260.
TEST(CookiePartitionKeyTest, NetworkIsolationKeyMode) {
  const net::SchemefulSite kTopFrameSite(GURL("https://a.com"));
  const net::SchemefulSite kFrameSite(GURL("https://b.com"));
  const auto kNonce = base::UnguessableToken::Create();

  {  // Frame site mode.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {net::features::kEnableCrossSiteFlagNetworkIsolationKey});

    const auto key = CookiePartitionKey::FromNetworkIsolationKey(
        NetworkIsolationKey(kTopFrameSite, kFrameSite, kNonce));
    EXPECT_TRUE(key);
    EXPECT_EQ(key->site(), kFrameSite);
    EXPECT_EQ(key->nonce().value(), kNonce);
  }

  {  // Cross-site flag mode.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {net::features::kEnableCrossSiteFlagNetworkIsolationKey}, {});

    const auto key = CookiePartitionKey::FromNetworkIsolationKey(
        NetworkIsolationKey(kTopFrameSite, kFrameSite, kNonce));
    EXPECT_TRUE(key);
    EXPECT_EQ(key->site(), kFrameSite);
    EXPECT_EQ(key->nonce().value(), kNonce);
  }
}

}  // namespace net
