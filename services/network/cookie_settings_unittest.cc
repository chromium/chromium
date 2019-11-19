// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";
constexpr char kDomain[] = "example.com";
constexpr char kDotDomain[] = ".example.com";
constexpr char kSubDomain[] = "www.corp.example.com";
constexpr char kOtherDomain[] = "not-example.com";
constexpr char kDomainWildcardPattern[] = "[*.]example.com";

ContentSettingPatternSource CreateSetting(const std::string& primary_pattern,
                                          const std::string& secondary_pattern,
                                          ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(primary_pattern),
      ContentSettingsPattern::FromString(secondary_pattern),
      base::Value(setting), std::string(), false /* incognito */);
}

TEST(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
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

TEST(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
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

TEST(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
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

TEST(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
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

TEST(CookieSettingsTest, LegacyCookieAccessDefault) {
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
TEST(CookieSettingsTest,
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
TEST(CookieSettingsTest,
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
TEST(CookieSettingsTest,
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
TEST(CookieSettingsTest,
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
