// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include <string>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_switches.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using enum CookiePartitionKey::AncestorChainBit;

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
      /*malformed site*/
      {"https://toplevelsite.com/", true, std::nullopt},
      /*valid site: cross site*/
      {"https://toplevelsite.com", true,
       CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"))},
      /*valid site: same site*/
      {"https://toplevelsite.com", false,
       CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"),
                                             kSameSite)}};
  for (const auto& tc : cases) {
    base::expected<std::optional<CookiePartitionKey>, std::string> got =
        CookiePartitionKey::FromStorage(tc.top_level_site, tc.third_party);
    EXPECT_EQ(got.has_value(), tc.expected_output.has_value());
    if (!tc.top_level_site.empty() && tc.expected_output.has_value()) {
      ASSERT_TRUE(got.has_value()) << "Expected result to have value.";
      EXPECT_EQ(got.value()->IsThirdParty(), tc.third_party);
    }
  }

  {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        kDisablePartitionedCookiesSwitch);
    EXPECT_FALSE(
        CookiePartitionKey::FromStorage("https://toplevelsite.com",
                                        /*has_cross_site_ancestor=*/true)
            .has_value());
  }
}

TEST_P(CookiePartitionKeyTest, TestFromUntrustedInput) {
  const std::string kFullURL = "https://subdomain.toplevelsite.com/index.html";
  const std::string kValidSite = "https://toplevelsite.com";
  struct Output {
    bool third_party;
  };
  struct {
    std::string top_level_site;
    CookiePartitionKey::AncestorChainBit has_cross_site_ancestor;
    std::optional<Output> expected_output;
  } cases[] = {
      {/*empty site*/
       "", kCrossSite, std::nullopt},
      {/*empty site : same site ancestor*/
       "", kSameSite, std::nullopt},
      {/*valid site*/
       kValidSite, kCrossSite, Output{true}},
      {/*valid site: same site ancestor*/
       kValidSite, kSameSite, Output{false}},
      {/*valid site with extra slash: same site ancestor*/
       kValidSite + "/", kSameSite, Output{false}},
      {/*invalid site (missing scheme)*/
       "toplevelsite.com", kCrossSite, std::nullopt},
      {/*invalid site (missing scheme): same site ancestor*/
       "toplevelsite.com", kSameSite, std::nullopt},
      {/*invalid site*/
       "abc123foobar!!", kCrossSite, std::nullopt},
      {/*invalid site: same site ancestor*/
       "abc123foobar!!", kSameSite, std::nullopt},
  };

  for (const auto& tc : cases) {
    base::expected<CookiePartitionKey, std::string> got =
        CookiePartitionKey::FromUntrustedInput(
            tc.top_level_site, tc.has_cross_site_ancestor == kCrossSite);
    EXPECT_EQ(got.has_value(), tc.expected_output.has_value());
    if (tc.expected_output.has_value()) {
      EXPECT_EQ(got->site().Serialize(), kValidSite);
      EXPECT_EQ(got->IsThirdParty(), tc.expected_output->third_party);
    }
  }

  {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        kDisablePartitionedCookiesSwitch);
    EXPECT_FALSE(
        CookiePartitionKey::FromUntrustedInput("https://toplevelsite.com",
                                               /*has_cross_site_ancestor=*/true)
            .has_value());
  }
}

TEST_P(CookiePartitionKeyTest, Serialization) {
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  struct Output {
    std::string top_level_site;
    bool cross_site;
  };
  struct {
    std::optional<CookiePartitionKey> input;
    std::optional<Output> expected_output;
  } cases[] = {
      // No partition key
      {std::nullopt, Output{kEmptyCookiePartitionKey, true}},
      // Partition key present
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com")),
       Output{"https://toplevelsite.com", true}},
      // Local file URL
      {CookiePartitionKey::FromURLForTesting(GURL("file:///path/to/file.txt")),
       Output{"file://", true}},
      // File URL with host
      {CookiePartitionKey::FromURLForTesting(
           GURL("file://toplevelsite.com/path/to/file.pdf")),
       Output{"file://toplevelsite.com", true}},
      // Opaque origin
      {CookiePartitionKey::FromURLForTesting(GURL()), std::nullopt},
      // AncestorChain::kSameSite
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"),
                                             kSameSite, std::nullopt),
       Output{"https://toplevelsite.com", false}},
      // AncestorChain::kCrossSite
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"),
                                             kCrossSite, std::nullopt),
       Output{"https://toplevelsite.com", true}},
      // With nonce
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://cookiesite.com")),
                               nonce),
           SiteForCookies::FromUrl(GURL::EmptyGURL()),
           SchemefulSite(GURL("https://toplevelsite.com")),
           /*main_frame_navigation=*/false),
       std::nullopt},
      // Same site no nonce from NIK
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://toplevelsite.com")),
           /*main_frame_navigation=*/false),
       Output{"https://toplevelsite.com", false}},
      // Different request_site results in cross site ancestor
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://differentOrigin.com")),
           /*main_frame_navigation=*/false),
       Output{"https://toplevelsite.com", true}},
      // Different request_site but main_frame_navigation=true results in same
      // site ancestor
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://differentOrigin.com")),
           /*main_frame_navigation=*/true),
       Output{"https://toplevelsite.com", false}},
      // Different request_site  and null site_for_cookies but
      // main_frame_navigation=true results in same
      // site ancestor
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com"))),
           SiteForCookies::FromUrl(GURL()),
           SchemefulSite(GURL("https://differentOrigin.com")),
           /*main_frame_navigation=*/true),
       Output{"https://toplevelsite.com", false}},
      // Same site with nonce from NIK
      {CookiePartitionKey::FromNetworkIsolationKey(
           NetworkIsolationKey(SchemefulSite(GURL("https://toplevelsite.com")),
                               SchemefulSite(GURL("https://toplevelsite.com")),
                               nonce),
           SiteForCookies::FromUrl(GURL("https://toplevelsite.com")),
           SchemefulSite(GURL("https://toplevelsite.com")),
           /*main_frame_navigation=*/false),
       std::nullopt},
      // Invalid partition key
      {std::make_optional(
           CookiePartitionKey::FromURLForTesting(GURL("abc123foobar!!"))),
       std::nullopt},
  };

  for (const auto& tc : cases) {
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        got = CookiePartitionKey::Serialize(tc.input);

    EXPECT_EQ(tc.expected_output.has_value(), got.has_value());
    if (got.has_value()) {
      EXPECT_EQ(tc.expected_output->top_level_site, got->TopLevelSite());
      EXPECT_EQ(tc.expected_output->cross_site, got->has_cross_site_ancestor());
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
    const bool main_frame_navigation;
  } test_cases[] = {
      {"Empty", NetworkIsolationKey(), std::nullopt,
       SiteForCookies::FromUrl(GURL::EmptyGURL()), SchemefulSite(GURL("")),
       /*main_frame_navigation=*/false},
      {"WithTopLevelSite", NetworkIsolationKey(kTopLevelSite, kCookieSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
       SiteForCookies::FromUrl(GURL::EmptyGURL()), SchemefulSite(kTopLevelSite),
       /*main_frame_navigation=*/false},
      {"WithNonce", NetworkIsolationKey(kTopLevelSite, kCookieSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kCookieSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies::FromUrl(GURL::EmptyGURL()), SchemefulSite(kTopLevelSite),
       /*main_frame_navigation=*/false},
      {"WithCrossSiteAncestorSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite,
                                             std::nullopt),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kTopLevelSite), /*main_frame_navigation=*/false},
      {"Nonced first party NIK results in kCrossSite partition key",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kTopLevelSite), /*main_frame_navigation=*/false},
      {"WithCrossSiteAncestorNotSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             std::nullopt),
       SiteForCookies::FromUrl(GURL::EmptyGURL()), kCookieSite,
       /*main_frame_navigation=*/false},
      {"TestMainFrameNavigationParam",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite,
                                             std::nullopt),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kCookieSite), /*main_frame_navigation=*/true},
      {"PresenceOfNonceTakesPriorityOverMainFrameNavigation",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies::FromUrl(GURL(kTopLevelSite.GetURL())),
       SchemefulSite(kTopLevelSite), /*main_frame_navigation=*/true},
  };

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(
      features::kAncestorChainBitEnabledInPartitionedCookies,
      AncestorChainBitEnabled());

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);

    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromNetworkIsolationKey(
            test_case.network_isolation_key, test_case.site_for_cookies,
            test_case.request_site, test_case.main_frame_navigation);

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
      {GURL("https://foo.com"), std::nullopt, kCrossSite},
      {GURL("https://foo.com"), std::nullopt, kSameSite},
      {GURL(), std::nullopt, kCrossSite},
      {GURL("https://foo.com"), base::UnguessableToken::Create(), kCrossSite}};

  for (const auto& test_case : test_cases) {
    auto want = CookiePartitionKey::FromURLForTesting(
        test_case.url, test_case.ancestor_chain_bit, test_case.nonce);
    auto got = CookiePartitionKey::FromWire(
        want.site(), want.IsThirdParty() ? kCrossSite : kSameSite,
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
      {GURL("https://foo.com"), std::nullopt, kCrossSite},
      {GURL("https://foo.com"), std::nullopt, kSameSite},
      {GURL(), std::nullopt, kCrossSite},
      {GURL("https://foo.com"), base::UnguessableToken::Create(), kCrossSite}};

  for (const auto& test_case : test_cases) {
    auto want = CookiePartitionKey::FromURLForTesting(
        test_case.url, test_case.ancestor_chain_bit, test_case.nonce);
    std::optional<CookiePartitionKey> got =
        CookiePartitionKey::FromStorageKeyComponents(
            want.site(), want.IsThirdParty() ? kCrossSite : kSameSite,
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
      GURL("https://foo.com"), kSameSite, std::nullopt);
  CookiePartitionKey key2 = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.com"), kCrossSite, std::nullopt);

  EXPECT_EQ((key1 == key2), !AncestorChainBitEnabled());
  EXPECT_EQ(key1, CookiePartitionKey::FromURLForTesting(
                      GURL("https://foo.com"), kSameSite, std::nullopt));
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
      top_level_site, /*main_frame_navigation=*/false);
  EXPECT_TRUE(key1.has_value());

  auto key2 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce2), SiteForCookies(),
      top_level_site, /*main_frame_navigation=*/false);
  EXPECT_TRUE(key1.has_value() && key2.has_value());
  EXPECT_NE(key1, key2);

  auto key3 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1), SiteForCookies(),
      top_level_site, /*main_frame_navigation=*/false);
  EXPECT_EQ(key1, key3);
  // Confirm that nonce is evaluated before main_frame_navigation
  auto key4 = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site, nonce1), SiteForCookies(),
      top_level_site, /*main_frame_navigation=*/true);
  EXPECT_EQ(key1, key4);
  auto unnonced_key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site), SiteForCookies(),
      frame_site, /*main_frame_navigation=*/false);
  EXPECT_NE(key1, unnonced_key);
}

TEST_P(CookiePartitionKeyTest, Localhost) {
  SchemefulSite top_level_site(GURL("https://localhost:8000"));

  auto key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, top_level_site), SiteForCookies(),
      top_level_site, /*main_frame_navigation=*/false);
  EXPECT_TRUE(key.has_value());

  SchemefulSite frame_site(GURL("https://cookiesite.com"));
  key = CookiePartitionKey::FromNetworkIsolationKey(
      NetworkIsolationKey(top_level_site, frame_site), SiteForCookies(),
      top_level_site, /*main_frame_navigation=*/false);
  EXPECT_TRUE(key.has_value());
}

}  // namespace net
