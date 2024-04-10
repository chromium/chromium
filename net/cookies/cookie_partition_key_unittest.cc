// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class CookiePartitionKeyTest : public testing::TestWithParam<bool> {
 protected:
  // testing::Test
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kAncestorChainBitEnabledInPartitionedCookies,
        AncestorChainBitEnabled());
  }

  bool AncestorChainBitEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         CookiePartitionKeyTest,
                         ::testing::Bool());

TEST_P(CookiePartitionKeyTest, TestFromStorage) {
  struct {
    const std::string top_level_site;
    bool third_party;
    const std::optional<CookiePartitionKey> expected_output;
  } cases[] = {
      {/*empty site*/
       "", true, CookiePartitionKey::FromURLForTesting(GURL(""))},
      /*invalid site*/
      {"Invalid", true, std::nullopt},
      /*valid site: cross site*/
      {"https://toplevelsite.com", true,
       CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"))},
      /*valid site: same site*/
      {"https://toplevelsite.com", false,
       CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"),
           CookiePartitionKey::AncestorChainBit::kSameSite)}};
  for (const auto& tc : cases) {
    base::expected<std::optional<CookiePartitionKey>, std::string> got =
        CookiePartitionKey::FromStorage(tc.top_level_site, tc.third_party);
    EXPECT_EQ(got.has_value(), tc.expected_output.has_value());
    if (!tc.top_level_site.empty() && tc.expected_output.has_value()) {
      ASSERT_TRUE(got.has_value()) << "Expected result to have value.";
      EXPECT_EQ(got.value()->IsThirdParty(), tc.third_party);
    }
  }
}

TEST_P(CookiePartitionKeyTest, TestFromUntrustedInput) {
  const std::string kFullURL = "https://subdomain.toplevelsite.com/index.html";
  const std::string kValidSite = "https://toplevelsite.com";
  struct {
    std::string top_level_site;
    CookiePartitionKey::AncestorChainBit has_cross_site_ancestor;
    bool partition_key_created;
    bool expected_third_party;
  } cases[] = {
      {/*empty site*/
       "", CookiePartitionKey::AncestorChainBit::kCrossSite, false, true},
      {/*empty site : same site ancestor*/
       "", CookiePartitionKey::AncestorChainBit::kSameSite, false, false},
      {/*valid site*/
       kValidSite, CookiePartitionKey::AncestorChainBit::kCrossSite, true,
       true},
      {/*valid site: same site ancestor*/
       kValidSite, CookiePartitionKey::AncestorChainBit::kSameSite, true,
       false},
      {/*invalid site (missing scheme)*/
       "toplevelsite.com", CookiePartitionKey::AncestorChainBit::kCrossSite,
       false, true},
      {/*invalid site (missing scheme): same site ancestor*/
       "toplevelsite.com", CookiePartitionKey::AncestorChainBit::kSameSite,
       false, false},
      {/*invalid site*/
       "abc123foobar!!", CookiePartitionKey::AncestorChainBit::kCrossSite,
       false, true},
      {/*invalid site: same site ancestor*/
       "abc123foobar!!", CookiePartitionKey::AncestorChainBit::kSameSite, false,
       false},
  };

  for (const auto& tc : cases) {
    base::expected<CookiePartitionKey, std::string> got =
        CookiePartitionKey::FromUntrustedInput(
            tc.top_level_site,
            tc.has_cross_site_ancestor ==
                CookiePartitionKey::AncestorChainBit::kCrossSite);
    EXPECT_EQ(got.has_value(), tc.partition_key_created);
    if (tc.partition_key_created) {
      EXPECT_EQ(got->site().Serialize(), kValidSite);
      EXPECT_EQ(got->IsThirdParty(), tc.expected_third_party);
    }
  }
}

TEST_P(CookiePartitionKeyTest, Serialization) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  struct {
    std::optional<CookiePartitionKey> input;
    std::string expected_output_top_level_site;
    bool expected_success;
    bool expected_cross_site;
  } cases[] = {
      // No partition key
      {std::nullopt, kEmptyCookiePartitionKey, true, true},
      // Partition key present
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com")),
       "https://toplevelsite.com", true, true},
      // Local file URL
      {CookiePartitionKey::FromURLForTesting(GURL("file:///path/to/file.txt")),
       "file://", true, true},
      // File URL with host
      {CookiePartitionKey::FromURLForTesting(
           GURL("file://toplevelsite.com/path/to/file.pdf")),
       "file://toplevelsite.com", true, true},
      // Opaque origin
      {CookiePartitionKey::FromURLForTesting(GURL()), "", false, true},
      // AncestorChain::kSameSite
      {CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"),
           CookiePartitionKey::AncestorChainBit::kSameSite, std::nullopt),
       "https://toplevelsite.com", true, false},
      // AncestorChain::kCrossSite
      {CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"),
           CookiePartitionKey::AncestorChainBit::kCrossSite, std::nullopt),
       "https://toplevelsite.com", true, true},
      // With nonce
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://cookiesite.com")),
                               nonce),
           SiteForCookies::FromUrl(GURL::EmptyGURL()),
           SchemefulSite(GURL("https://toplevelsite.com"))),
       "", false, true},
      // Same site no nonce from NIK
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://toplevelsite.com"))),
       "https://toplevelsite.com", true, false},
      // Different request_site results in cross site ancestor
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://differentOrigin.com"))),
       "https://toplevelsite.com", true, true},
      // Same site with nonce from NIK
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com")),
                               nonce),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://toplevelsite.com"))),
       "", false, true},
      // Invalid partition key
      {std::make_optional(
           CookiePartitionKey::FromURLForTesting(GURL("abc123foobar!!"))),
       "", false, true},
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
      EXPECT_EQ(tc.expected_cross_site, got->has_cross_site_ancestor());
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
    const std::optional<CookiePartitionKey> expected;
    const SiteForCookies site_for_cookies;
    const SchemefulSite request_site;
  } test_cases[] = {
      {"Empty", NetworkIsolationKey(), std::nullopt,
       SiteForCookies::FromUrl(GURL::EmptyGURL()), SchemefulSite(GURL(""))},
      {"WithTopLevelSite", NetworkIsolationKey(kTopLevelSite, kCookieSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
       SiteForCookies::FromUrl(GURL::EmptyGURL()),
       SchemefulSite(kTopLevelSite)},
      {"WithNonce", NetworkIsolationKey(kTopLevelSite, kCookieSite, kNonce),
       CookiePartitionKey::FromURLForTesting(
           kCookieSite.GetURL(),
           CookiePartitionKey::AncestorChainBit::kCrossSite, kNonce),
       SiteForCookies::FromUrl(GURL::EmptyGURL()),
       SchemefulSite(kTopLevelSite)},
      {"WithCrossSiteAncestorSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(
           kTopLevelSite.GetURL(),
           CookiePartitionKey::AncestorChainBit::kSameSite, std::nullopt),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kTopLevelSite)},
      {"Nonced first party NIK results in kCrossSite partition key",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite, kNonce),
       CookiePartitionKey::FromURLForTesting(
           kTopLevelSite.GetURL(),
           CookiePartitionKey::AncestorChainBit::kCrossSite, kNonce),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kTopLevelSite)},
      {"WithCrossSiteAncestorNotSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(
           kTopLevelSite.GetURL(),
           CookiePartitionKey::AncestorChainBit::kCrossSite, std::nullopt),
       SiteForCookies::FromUrl(GURL::EmptyGURL()), kCookieSite}};

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(
      features::kAncestorChainBitEnabledInPartitionedCookies,
      AncestorChainBitEnabled());

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);

    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            test_case.network_isolation_key, test_case.site_for_cookies,
            test_case.request_site);

    EXPECT_EQ(test_case.expected, got);
    if (got) {
      EXPECT_EQ(test_case.network_isolation_key.GetNonce(), got->nonce());
    }
  }
}

TEST_P(CookiePartitionKeyTest, FromWire) {
  struct TestCase {
    const GURL url;
    const std::optional<base::UnguessableToken> nonce;
    const CookiePartitionKey::AncestorChainBit ancestor_chain_bit;
  } test_cases[] = {
      {GURL("https://foo.com"), std::nullopt,
       CookiePartitionKey::AncestorChainBit::kCrossSite},
      {GURL("https://foo.com"), std::nullopt,
       CookiePartitionKey::AncestorChainBit::kSameSite},
      {GURL(), std::nullopt, CookiePartitionKey::AncestorChainBit::kCrossSite},
      {GURL("https://foo.com"), base::UnguessableToken::Create(),
       CookiePartitionKey::AncestorChainBit::kCrossSite}};

  for (const auto& test_case : test_cases) {
    auto want = CookiePartitionKey::FromURLForTesting(
        test_case.url, test_case.ancestor_chain_bit, test_case.nonce);
    auto got = CookiePartitionKey::FromWire(
        want.site(),
        want.IsThirdParty() ? CookiePartitionKey::AncestorChainBit::kCrossSite
                            : CookiePartitionKey::AncestorChainBit::kSameSite,
        want.nonce());
    EXPECT_EQ(want, got);
    EXPECT_FALSE(got.from_script());
  }
}

TEST_P(CookiePartitionKeyTest, FromStorageKeyComponents) {
  struct TestCase {
    const GURL url;
    const std::optional<base::UnguessableToken> nonce = std::nullopt;
    const CookiePartitionKey::AncestorChainBit ancestor_chain_bit;
  } test_cases[] = {
      {GURL("https://foo.com"), std::nullopt,
       CookiePartitionKey::AncestorChainBit::kCrossSite},
      {GURL("https://foo.com"), std::nullopt,
       CookiePartitionKey::AncestorChainBit::kSameSite},
      {GURL(), std::nullopt, CookiePartitionKey::AncestorChainBit::kCrossSite},
      {GURL("https://foo.com"), base::UnguessableToken::Create(),
       CookiePartitionKey::AncestorChainBit::kCrossSite}};

  for (const auto& test_case : test_cases) {
    auto want = CookiePartitionKey::FromURLForTesting(
        test_case.url, test_case.ancestor_chain_bit, test_case.nonce);
    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromStorageKeyComponents(
            want.site(),
            want.IsThirdParty()
                ? CookiePartitionKey::AncestorChainBit::kCrossSite
                : CookiePartitionKey::AncestorChainBit::kSameSite,
            want.nonce());
    EXPECT_EQ(got, want);
  }
}

TEST_P(CookiePartitionKeyTest, FromScript) {
  auto key = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key);
  EXPECT_TRUE(key->from_script());
  EXPECT_TRUE(key->site().opaque());
  EXPECT_TRUE(key->IsThirdParty());

  auto key2 = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key2);
  EXPECT_TRUE(key2->from_script());
  EXPECT_TRUE(key2->site().opaque());
  EXPECT_TRUE(key2->IsThirdParty());

  // The keys should not be equal because they get created with different opaque
  // sites. Test both the '==' and '!=' operators here.
  EXPECT_FALSE(key == key2);
  EXPECT_TRUE(key != key2);
}

TEST_P(CookiePartitionKeyTest, IsSerializeable) {
  EXPECT_FALSE(CookiePartitionKey::FromURLForTesting(GURL()).IsSerializeable());
  EXPECT_TRUE(
      CookiePartitionKey::FromURLForTesting(GURL("https://www.example.com"))
          .IsSerializeable());
}

TEST_P(CookiePartitionKeyTest, Equality) {
  // Same eTLD+1 but different scheme are not equal.
  EXPECT_NE(CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")),
            CookiePartitionKey::FromURLForTesting(GURL("http://foo.com")));

  // Different subdomains of the same site are equal.
  EXPECT_EQ(CookiePartitionKey::FromURLForTesting(GURL("https://a.foo.com")),
            CookiePartitionKey::FromURLForTesting(GURL("https://b.foo.com")));
}

TEST_P(CookiePartitionKeyTest, Equality_WithAncestorChain) {
  CookiePartitionKey key1 = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.com"), CookiePartitionKey::AncestorChainBit::kSameSite,
      std::nullopt);
  CookiePartitionKey key2 = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.com"), CookiePartitionKey::AncestorChainBit::kCrossSite,
      std::nullopt);

  EXPECT_EQ((key1 == key2), !AncestorChainBitEnabled());
  EXPECT_EQ(key1,
            CookiePartitionKey::FromURLForTesting(
                GURL("https://foo.com"),
                CookiePartitionKey::AncestorChainBit::kSameSite, std::nullopt));
}

TEST_P(CookiePartitionKeyTest, Equality_WithNonce) {
  SchemefulSite top_level_site =
      SchemefulSite(GURL("https://toplevelsite.com"));
  SchemefulSite frame_site = SchemefulSite(GURL("https://cookiesite.com"));
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  EXPECT_NE(nonce1, nonce2);
  auto key1 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1), SiteForCookies(),
      top_level_site);
  EXPECT_TRUE(key1.has_value());

  auto key2 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce2), SiteForCookies(),
      top_level_site);
  EXPECT_TRUE(key1.has_value() && key2.has_value());
  EXPECT_NE(key1, key2);

  auto key3 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1), SiteForCookies(),
      top_level_site);
  EXPECT_EQ(key1, key3);

  auto unnonced_key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site), SiteForCookies(),
      frame_site);
  EXPECT_NE(key1, unnonced_key);
}

TEST_P(CookiePartitionKeyTest, Localhost) {
  SchemefulSite top_level_site(GURL("https://localhost:8000"));

  auto key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, top_level_site), SiteForCookies(),
      top_level_site);
  EXPECT_TRUE(key.has_value());

  SchemefulSite frame_site(GURL("https://cookiesite.com"));
  key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site), SiteForCookies(),
      top_level_site);
  EXPECT_TRUE(key.has_value());
}

// Test that creating nonced partition keys works with both types of
// NetworkIsolationKey modes. See https://crbug.com/1442260.
TEST_P(CookiePartitionKeyTest, NetworkIsolationKeyMode) {
  const net::SchemefulSite kTopFrameSite(GURL("https://a.com"));
  const net::SchemefulSite kFrameSite(GURL("https://b.com"));
  const auto kNonce = base::UnguessableToken::Create();

  SiteForCookies site_for_cookies =
      SiteForCookies::FromUrl(GURL("https://a.com"));

  {  // Frame site mode.
    base::test::ScopedFeatureList feature_list;

    feature_list.InitWithFeatureState(
        features::kEnableCrossSiteFlagNetworkIsolationKey, false);

    const auto key = CookiePartitionKey::FromNetworkIsolationKey(
        NetworkIsolationKey(kTopFrameSite, kFrameSite, kNonce),
        site_for_cookies, kTopFrameSite);
    EXPECT_TRUE(key);
    EXPECT_EQ(key->site(), kFrameSite);
    EXPECT_EQ(key->nonce().value(), kNonce);
    EXPECT_TRUE(key->IsThirdParty());
  }

  {  // Cross-site flag mode.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureStates(
        {{net::features::kEnableCrossSiteFlagNetworkIsolationKey, true},
         {features::kAncestorChainBitEnabledInPartitionedCookies,
          AncestorChainBitEnabled()}});

    const auto key = CookiePartitionKey::FromNetworkIsolationKey(
        NetworkIsolationKey(kTopFrameSite, kFrameSite, kNonce),
        site_for_cookies, kTopFrameSite);
    EXPECT_TRUE(key);
    EXPECT_EQ(key->site(), kFrameSite);
    EXPECT_EQ(key->nonce().value(), kNonce);
    EXPECT_TRUE(key->IsThirdParty());
  }
}
}  // namespace net
