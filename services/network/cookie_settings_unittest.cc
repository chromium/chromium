// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests";

constexpr char kDomainURL[] = "http://example.com";
constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";
constexpr char kSubDomainURL[] = "http://www.corp.example.com";
constexpr char kDomain[] = "example.com";
constexpr char kDotDomain[] = ".example.com";
constexpr char kSubDomain[] = "www.corp.example.com";
constexpr char kOtherDomain[] = "not-example.com";
constexpr char kDomainWildcardPattern[] = "[*.]example.com";

class CookieSettingsTest : public testing::Test {
 public:
 public:
  CookieSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ContentSettingPatternSource CreateSetting(
      const std::string& primary_pattern,
      const std::string& secondary_pattern,
      ContentSetting setting,
      base::Time expiration = base::Time()) {
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromString(primary_pattern),
        ContentSettingsPattern::FromString(secondary_pattern),
        base::Value(setting), std::string(), false /* incognito */, expiration);
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST_F(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

TEST_F(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

// The Storage Access API should unblock storage access that would otherwise be
// blocked.
TEST_F(CookieSettingsTest, GetCookieSettingSAAUnblocks) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  GURL third_url = GURL(kDomainURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  ContentSetting setting;
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  settings.GetCookieSetting(top_level_url, url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
      1);

  // Invalid pairs where a |third_url| is used.
  settings.GetCookieSetting(url, third_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
  settings.GetCookieSetting(third_url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted embedding url should not gain access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kDomainURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(kDomain, top_level_url.host(), CONTENT_SETTING_ALLOW)});

  ContentSetting setting;
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(GURL(kSubDomainURL), top_level_url, nullptr,
                            &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_F(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  GURL top_level_url = GURL(kDomainURL);
  GURL url = GURL(kURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), kDomain, CONTENT_SETTING_ALLOW)});

  ContentSetting setting;
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(url, GURL(kSubDomainURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_F(CookieSettingsTest, GetCookieSettingSAARespectsSettings) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  ContentSetting setting;
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_F(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  base::Time expiration_time =
      base::Time::Now() + base::TimeDelta::FromSeconds(100);
  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW,
                     expiration_time)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  ContentSetting setting;
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::TimeDelta::FromSeconds(101));
  settings.GetCookieSetting(url, top_level_url, nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_F(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
  CookieSettings settings;
  settings.set_secure_origin_cookies_allowed_schemes({"chrome"});
  settings.set_block_third_party_cookies(true);

  ContentSetting setting;
  settings.GetCookieSetting(GURL("https://foo.com") /* url */,
                            GURL("chrome://foo") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(GURL("chrome://foo") /* url */,
                            GURL("https://foo.com") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);

  settings.GetCookieSetting(GURL("http://foo.com") /* url */,
                            GURL("chrome://foo") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
  CookieSettings settings;
  settings.set_third_party_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  ContentSetting setting;
  settings.GetCookieSetting(
      GURL("http://foo.com") /* url */,
      GURL("chrome-extension://foo") /* first_party_url */,
      nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(GURL("http://foo.com") /* url */,
                            GURL("other-scheme://foo") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);

  settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                            GURL("http://foo.com") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
  CookieSettings settings;
  settings.set_matching_scheme_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  ContentSetting setting;
  settings.GetCookieSetting(
      GURL("chrome-extension://bar") /* url */,
      GURL("chrome-extension://foo") /* first_party_url */,
      nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(
      GURL("http://foo.com") /* url */,
      GURL("chrome-extension://foo") /* first_party_url */,
      nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);

  settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                            GURL("http://foo.com") /* first_party_url */,
                            nullptr /* source */, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, LegacyCookieAccessDefault) {
  CookieSettings settings;
  ContentSetting setting;

  // Test SameSite-by-default enabled (default semantics is NONLEGACY)
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);
    settings.GetSettingForLegacyCookieAccess(kDomain, &setting);
    EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
    EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
              settings.GetCookieAccessSemanticsForDomain(kDomain));
  }

  // Test SameSite-by-default disabled (default semantics is LEGACY)
  // TODO(crbug.com/953306): Remove this when legacy code path is removed.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        net::features::kSameSiteByDefaultCookies);
    settings.GetSettingForLegacyCookieAccess(kDomain, &setting);
    EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
    EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
              settings.GetCookieAccessSemanticsForDomain(kDomain));
  }
}

// Test SameSite-by-default disabled (default semantics is LEGACY)
// TODO(crbug.com/953306): Remove this when legacy code path is removed.
TEST_F(CookieSettingsTest,
       CookieAccessSemanticsForDomain_SameSiteByDefaultDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kSameSiteByDefaultCookies);
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomain, "*", CONTENT_SETTING_BLOCK)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are NONLEGACY because they match the setting.
      {net::CookieAccessSemantics::NONLEGACY, kDomain},
      {net::CookieAccessSemantics::NONLEGACY, kDotDomain},
      // These two test cases default into LEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      {net::CookieAccessSemantics::LEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

// Test SameSite-by-default enabled (default semantics is NONLEGACY)
TEST_F(CookieSettingsTest,
       CookieAccessSemanticsForDomain_SameSiteByDefaultEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomain, "*", CONTENT_SETTING_ALLOW)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // These two test cases default into NONLEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

// Test SameSite-by-default disabled (default semantics is LEGACY)
// TODO(crbug.com/953306): Remove this when legacy code path is removed.
TEST_F(CookieSettingsTest,
       CookieAccessSemanticsForDomainWithWildcard_SameSiteByDefaultDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kSameSiteByDefaultCookies);
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomainWildcardPattern, "*", CONTENT_SETTING_BLOCK)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are NONLEGACY because they match the setting.
      {net::CookieAccessSemantics::NONLEGACY, kDomain},
      {net::CookieAccessSemantics::NONLEGACY, kDotDomain},
      // Subdomain also matches pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      // This test case defaults into LEGACY.
      {net::CookieAccessSemantics::LEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

// Test SameSite-by-default enabled (default semantics is NONLEGACY)
TEST_F(CookieSettingsTest,
       CookieAccessSemanticsForDomainWithWildcard_SameSiteByDefaultEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);
  CookieSettings settings;
  settings.set_content_settings_for_legacy_cookie_access(
      {CreateSetting(kDomainWildcardPattern, "*", CONTENT_SETTING_ALLOW)});
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // Subdomain also matches pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      // This test case defaults into NONLEGACY.
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status,
              settings.GetCookieAccessSemanticsForDomain(test.cookie_domain));
  }
}

}  // namespace
}  // namespace network
