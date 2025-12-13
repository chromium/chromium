// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include <optional>
#include <string>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/network_isolation_partition.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

using enum CookiePartitionKey::AncestorChainBit;

class CookiePartitionKeyTest : public ::testing::Test {};


TEST(CookiePartitionKeyTest, TestFromStorage) {
  struct {
    std::string top_level_site;
    bool third_party;
    base::expected<std::optional<CookiePartitionKey>, std::string>
        expected_output;
  } cases[] = {
      {/*empty site*/
       "", true, base::ok(std::nullopt)},
      /*invalid site*/
      {"Invalid", true,
       base::unexpected(
           "Cannot deserialize opaque origin to CookiePartitionKey")},
      /*malformed site*/
      {"https://toplevelsite.com/", true,
       base::unexpected("Cannot deserialize malformed top_level_site to "
                        "CookiePartitionKey")},
      /*valid site: cross site*/
      {"https://toplevelsite.com", true,
       base::ok(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com")))},
      /*valid site: same site*/
      {"https://toplevelsite.com", false,
       base::ok(CookiePartitionKey::FromURLForTesting(
           GURL("https://toplevelsite.com"), kSameSite))},
  };
  for (const auto& tc : cases) {
    base::expected<std::optional<CookiePartitionKey>, std::string> got =
        CookiePartitionKey::FromStorage(tc.top_level_site, tc.third_party);
    EXPECT_EQ(tc.expected_output, got) << got.ToString();
  }

#if BUILDFLAG(IS_ANDROID)
  {
    base::AutoReset<bool> reset =
        CookiePartitionKey::DisablePartitioningInScopeForTesting();
    EXPECT_FALSE(
        CookiePartitionKey::FromStorage("https://toplevelsite.com",
                                        /*has_cross_site_ancestor=*/true)
            .has_value());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST(CookiePartitionKeyTest, TestFromUntrustedInput) {
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

#if BUILDFLAG(IS_ANDROID)
  {
    base::AutoReset<bool> reset =
        CookiePartitionKey::DisablePartitioningInScopeForTesting();
    EXPECT_FALSE(
        CookiePartitionKey::FromUntrustedInput("https://toplevelsite.com",
                                               /*has_cross_site_ancestor=*/true)
            .has_value());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST(CookiePartitionKeyTest, Serialization) {
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
                                             kSameSite),
       Output{"https://toplevelsite.com", false}},
      // AncestorChain::kCrossSite
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"),
                                             kCrossSite),
       Output{"https://toplevelsite.com", true}},
      // With nonce
      {CookiePartitionKey::FromURLForTesting(GURL("https://cookiesite.com"),
                                             kCrossSite, nonce),
       std::nullopt},
      // Same site w/ nonce
      {CookiePartitionKey::FromURLForTesting(GURL("https://toplevelsite.com"),
                                             kSameSite, nonce),
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

TEST(CookiePartitionKeyTest, FromNetworkIsolationKey) {
  const SchemefulSite kTopLevelSite(GURL("https://toplevelsite.com"));
  const SchemefulSite kCookieSite(GURL("https://cookiesite.com"));
  const SchemefulSite kLocalhost(GURL("https://localhost:8000"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

  struct TestCase {
    const std::string desc;
    const NetworkIsolationKey network_isolation_key;
    const std::optional<CookiePartitionKey> expected;
    const SiteForCookies site_for_cookies;
    const SchemefulSite request_site;
    const bool main_frame_navigation;
  } test_cases[] = {
      {"Empty", NetworkIsolationKey(), std::nullopt, SiteForCookies(),
       SchemefulSite(),
       /*main_frame_navigation=*/false},
      {"WithTopLevelSite", NetworkIsolationKey(kTopLevelSite, kCookieSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL()),
       SiteForCookies(), kTopLevelSite,
       /*main_frame_navigation=*/false},
      {"WithNonce", NetworkIsolationKey(kTopLevelSite, kCookieSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kCookieSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies(), kTopLevelSite,
       /*main_frame_navigation=*/false},
      {"WithNetworkIsolationPartition",
       NetworkIsolationKey(
           kTopLevelSite, kCookieSite, /*nonce=*/std::nullopt,
           NetworkIsolationPartition::kProtectedAudienceSellerWorklet),
       std::nullopt, SiteForCookies(), kTopLevelSite,
       /*main_frame_navigation=*/false},
      {"WithCrossSiteAncestorSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite,
                                             std::nullopt),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kTopLevelSite,
       /*main_frame_navigation=*/false},
      {"Nonced first party NIK results in kCrossSite partition key",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kTopLevelSite,
       /*main_frame_navigation=*/false},
      {"WithCrossSiteAncestorNotSameSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             std::nullopt),
       SiteForCookies(), kCookieSite,
       /*main_frame_navigation=*/false},
      {"TestMainFrameNavigationParam",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite,
                                             std::nullopt),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kCookieSite,
       /*main_frame_navigation=*/true},
      {"PresenceOfNonceTakesPriorityOverMainFrameNavigation",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite, kNonce),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kCrossSite,
                                             kNonce),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kTopLevelSite,
       /*main_frame_navigation=*/true},
      {"LocalhostABA", NetworkIsolationKey(kLocalhost, kLocalhost),
       CookiePartitionKey::FromURLForTesting(kLocalhost.GetURL(), kCrossSite,
                                             std::nullopt),
       SiteForCookies(), kLocalhost,
       /*main_frame_navigation=*/false},
      {"LocalhostCrossSite", NetworkIsolationKey(kLocalhost, kCookieSite),
       CookiePartitionKey::FromURLForTesting(kLocalhost.GetURL(), kCrossSite,
                                             std::nullopt),
       SiteForCookies(), kLocalhost,
       /*main_frame_navigation=*/false},
      // Different request_site results in cross site ancestor
      {"DifferentRequestSite",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(),
                                             kCrossSite),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kCookieSite,
       /*main_frame_navigation=*/false},
      {"DifferentRequestSiteMainFrameNavigation",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite),
       SiteForCookies::FromUrl(kTopLevelSite.GetURL()), kCookieSite,
       /*main_frame_navigation=*/true},
      {"DifferentRequestSiteMainFrameNavigationNullSiteForCookies",
       NetworkIsolationKey(kTopLevelSite, kTopLevelSite),
       CookiePartitionKey::FromURLForTesting(kTopLevelSite.GetURL(), kSameSite),
       SiteForCookies(), kCookieSite,
       /*main_frame_navigation=*/true},
  };

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

TEST(CookiePartitionKeyTest, FromWire) {
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

TEST(CookiePartitionKeyTest, FromStorageKeyComponents) {
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

TEST(CookiePartitionKeyTest, FromScript) {
  auto key = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key.from_script());
  EXPECT_TRUE(key.site().opaque());
  EXPECT_TRUE(key.IsThirdParty());

  auto key2 = CookiePartitionKey::FromScript();
  EXPECT_TRUE(key2.from_script());
  EXPECT_TRUE(key2.site().opaque());
  EXPECT_TRUE(key2.IsThirdParty());

  EXPECT_EQ(key, key2);
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

TEST(CookiePartitionKeyTest, Equality_WithAncestorChain) {
  CookiePartitionKey key1 = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.com"), kSameSite, std::nullopt);
  CookiePartitionKey key2 = CookiePartitionKey::FromURLForTesting(
      GURL("https://foo.com"), kCrossSite, std::nullopt);

  EXPECT_NE(key1 , key2);
  EXPECT_EQ(key1, CookiePartitionKey::FromURLForTesting(
                      GURL("https://foo.com"), kSameSite, std::nullopt));
}

TEST(CookiePartitionKeyTest, Equality_WithNonce) {
  GURL frame_url("https://cookiesite.com");
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  ASSERT_NE(nonce1, nonce2);

  CookiePartitionKey key1 =
      CookiePartitionKey::FromURLForTesting(frame_url, kCrossSite, nonce1);
  CookiePartitionKey key2 =
      CookiePartitionKey::FromURLForTesting(frame_url, kCrossSite, nonce2);
  EXPECT_NE(key1, key2);

  CookiePartitionKey key3 =
      CookiePartitionKey::FromURLForTesting(frame_url, kCrossSite, nonce1);
  EXPECT_EQ(key1, key3);
  CookiePartitionKey unnonced_key =
      CookiePartitionKey::FromURLForTesting(frame_url, kCrossSite);
  EXPECT_NE(key1, unnonced_key);
}

TEST(CookiePartitionKeyTest, NoncedKeyForbidsUnpartitionedAccess) {
  GURL frame_url("https://cookiesite.com");

  EXPECT_FALSE(
      CookiePartitionKey::FromURLForTesting(frame_url, kCrossSite, std::nullopt)
          .ForbidsUnpartitionedCookieAccess());

  EXPECT_TRUE(CookiePartitionKey::FromURLForTesting(
                  frame_url, kCrossSite, base::UnguessableToken::Create())
                  .ForbidsUnpartitionedCookieAccess());
}

}  // namespace net
