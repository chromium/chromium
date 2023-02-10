// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";

constexpr char kDomainURL[] = "http://example.com";
constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";
constexpr char kSubDomainURL[] = "http://www.corp.example.com";
constexpr char kDomain[] = "example.com";
constexpr char kDotDomain[] = ".example.com";
constexpr char kSubDomain[] = "www.corp.example.com";
constexpr char kOtherDomain[] = "not-example.com";
constexpr char kDomainWildcardPattern[] = "[*.]example.com";
constexpr char kFPSOwnerURL[] = "https://fps-owner.test";
constexpr char kFPSMemberURL[] = "https://fps-member.test";
constexpr char kUnrelatedURL[] = "http://unrelated.com";

std::unique_ptr<net::CanonicalCookie> MakeCanonicalCookie(
    const std::string& name,
    const std::string& domain,
    absl::optional<net::CookiePartitionKey> cookie_partition_key =
        absl::nullopt) {
  return net::CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "1", domain, /*path=*/"/", /*creation=*/base::Time(),
      /*expiration=*/base::Time(), /*last_access=*/base::Time(),
      /*last_update=*/base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
      net::CookiePriority::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false,
      cookie_partition_key);
}

struct TestCase {
  std::string test_name;
  bool storage_access_grant_eligible;
  bool top_level_storage_access_grant_eligible;
  bool force_allow_third_party_cookies;
};

class CookieSettingsTest : public testing::TestWithParam<TestCase> {
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
        base::Value(setting), std::string(), false /* incognito */,
        {.expiration = expiration});
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  bool IsStorageAccessGrantEligible() const {
    return GetParam().storage_access_grant_eligible;
  }

  bool IsTopLevelStorageAccessGrantEligible() const {
    return GetParam().top_level_storage_access_grant_eligible;
  }

  bool IsForceAllowThirdPartyCookies() const {
    return GetParam().force_allow_third_party_cookies;
  }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    if (IsStorageAccessGrantEligible()) {
      overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
    }
    if (IsTopLevelStorageAccessGrantEligible()) {
      overrides.Put(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    }
    if (IsForceAllowThirdPartyCookies()) {
      overrides.Put(net::CookieSettingOverride::kForceThirdPartyByUser);
    }
    return overrides;
  }

  // Assumes that cookie access would be blocked if not for a Storage Access API
  // grant or force allow. The `allow` parameter indicates the setting to be
  // returned if cookie access is expected to be allowed.
  ContentSetting SettingWithEitherOverride(ContentSetting allow) const {
    DCHECK(allow == CONTENT_SETTING_ALLOW ||
           allow == CONTENT_SETTING_SESSION_ONLY);
    return IsStorageAccessGrantEligible() || IsForceAllowThirdPartyCookies()
               ? allow
               : CONTENT_SETTING_BLOCK;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant, and user force allow.
  ContentSetting SettingWithEitherOverrideForTopLevel() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    return IsTopLevelStorageAccessGrantEligible() ||
                   IsForceAllowThirdPartyCookies()
               ? CONTENT_SETTING_ALLOW
               : CONTENT_SETTING_BLOCK;
  }

  ContentSetting SettingWithForceAllowThirdPartyCookies() const {
    return IsForceAllowThirdPartyCookies() ? CONTENT_SETTING_ALLOW
                                           : CONTENT_SETTING_BLOCK;
  }

  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithForceAllowThirdPartyCookies() const {
    return IsForceAllowThirdPartyCookies()
               ? net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED
               : net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The cookie access result would be blocked if not for a Storage Access API
  // grant or force allow.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithEitherOverride() const {
    if (IsStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    if (IsForceAllowThirdPartyCookies()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant, and user force allow to allow cookie
  // access.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithEitherOverrideForTopLevel() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    if (IsTopLevelStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    }
    if (IsForceAllowThirdPartyCookies()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
}

TEST_P(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  // This is blocked and not forced by override, because the override
  // does not apply to a block by pattern match.
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

TEST_P(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest,
       GetCookieSettingOverridePreservesSessionOnlySetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});

  settings.set_storage_access_grants(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride(CONTENT_SETTING_SESSION_ONLY));
}

TEST_P(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
}

// The Storage Access API should unblock storage access that would otherwise be
// blocked.
TEST_P(CookieSettingsTest, GetCookieSettingSAAUnblocks) {
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
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride(CONTENT_SETTING_ALLOW));
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithEitherOverride()), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_STORAGE_ACCESS_GRANT),
      IsStorageAccessGrantEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithEitherOverride()),
      IsStorageAccessGrantEligible() ? 1 : 2);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());

  // If third-party cookies are blocked, SAA grant takes precedence over
  // possible override to force allow 3PCs.
  {
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              SettingWithEitherOverride(CONTENT_SETTING_ALLOW));
    histogram_tester_2.ExpectTotalCount(kAllowedRequestsHistogram, 1);
    histogram_tester_2.ExpectBucketCount(
        kAllowedRequestsHistogram,
        static_cast<int>(BlockedStorageAccessResultWithEitherOverride()), 1);
  }

  // If cookies are globally blocked, SAA grants and 3PC override
  // should both be ignored.
  {
    settings.set_content_settings(
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectTotalCount(kAllowedRequestsHistogram, 1);
    histogram_tester_2.ExpectBucketCount(
        kAllowedRequestsHistogram,
        static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
        1);
  }
}

// The Top-Level Storage Access API should unblock storage access that would
// otherwise be blocked.
TEST_P(CookieSettingsTest, GetCookieSettingTopLevelStorageAccessUnblocks) {
  const GURL top_level_url(kURL);
  const GURL url(kOtherURL);
  const GURL third_url(kDomainURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  // Only set the storage access granted by Top-Level Storage Access API.
  settings.set_top_level_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverrideForTopLevel());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(
          BlockedStorageAccessResultWithEitherOverrideForTopLevel()),
      1);

  // Check the cookie setting that does not match the top-level storage access
  // grant--the |top_level_url| granting access to |url| is now being loaded
  // under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  // TODO(crbug.com/1385156): Separate metrics between StorageAccessAPI
  // and the page-level variant.
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::
                           ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT),
      IsTopLevelStorageAccessGrantEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(
          BlockedStorageAccessResultWithEitherOverrideForTopLevel()),
      IsTopLevelStorageAccessGrantEligible() ? 1 : 2);

  // Check the cookie setting that does not match the top-level storage access
  // grant where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());

  // If third-party cookies are blocked, Top-Level Storage Access grant takes
  // precedence over possible override to force allow third-party cookies.
  {
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              SettingWithEitherOverrideForTopLevel());
    histogram_tester_2.ExpectTotalCount(kAllowedRequestsHistogram, 1);
    histogram_tester_2.ExpectBucketCount(
        kAllowedRequestsHistogram,
        static_cast<int>(
            BlockedStorageAccessResultWithEitherOverrideForTopLevel()),
        1);
  }

  // If cookies are globally blocked, Top-Level Storage Access grants and 3PC
  // override should both be ignored.
  {
    settings.set_content_settings(
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectTotalCount(kAllowedRequestsHistogram, 1);
    histogram_tester_2.ExpectBucketCount(
        kAllowedRequestsHistogram,
        static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_BLOCKED),
        1);
  }
}

// Subdomains of the granted embedding url should not gain access if a valid
// grant exists.
TEST_P(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kDomainURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(kDomain, top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(settings.GetCookieSetting(GURL(kSubDomainURL), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_P(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  GURL top_level_url = GURL(kDomainURL);
  GURL url = GURL(kURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), kDomain, CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(settings.GetCookieSetting(url, GURL(kSubDomainURL),
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_P(CookieSettingsTest, GetCookieSettingSAARespectsSettings) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_P(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  base::Time expiration_time = base::Time::Now() + base::Seconds(100);
  settings.set_storage_access_grants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW,
                     expiration_time)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride(CONTENT_SETTING_ALLOW));

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_P(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
  CookieSettings settings;
  settings.set_secure_origin_cookies_allowed_schemes({"chrome"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("https://foo.com") /* url */,
                GURL("chrome://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome://foo") /* url */,
                GURL("https://foo.com") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
  CookieSettings settings;
  settings.set_third_party_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("other-scheme://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://foo") /* url */,
                GURL("http://foo.com") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
  CookieSettings settings;
  settings.set_matching_scheme_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://bar") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://foo") /* url */,
                GURL("http://foo.com") /* first_party_url */,
                GetCookieSettingOverrides(), nullptr /* source */),
            SettingWithForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest, LegacyCookieAccessDefault) {
  CookieSettings settings;

  EXPECT_EQ(settings.GetSettingForLegacyCookieAccess(kDomain),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            settings.GetCookieAccessSemanticsForDomain(kDomain));
}

TEST_P(CookieSettingsTest, CookieAccessSemanticsForDomain) {
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

TEST_P(CookieSettingsTest, CookieAccessSemanticsForDomainWithWildcard) {
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

TEST_P(CookieSettingsTest, IsPrivacyModeEnabled) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Third-party requests should only have accessed to partitioned state.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          net::CookieSettingOverrides()));

  // Same for requests with a null site_for_cookies, even if the
  // top_frame_origin matches.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kURL)),
                                          net::CookieSettingOverrides()));

  // The first party is able to send any type of state.
  EXPECT_EQ(
      net::NetworkDelegate::PrivacySetting::kStateAllowed,
      settings.IsPrivacyModeEnabled(
          GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
          url::Origin::Create(GURL(kURL)), net::CookieSettingOverrides()));

  // Setting a site-specific rule for the top-level frame origin that blocks
  // access should cause partitioned state to be disallowed.
  settings.set_content_settings(
      {CreateSetting(kOtherURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          net::CookieSettingOverrides()));

  // Setting a site-specific rule for the top-level frame origin when it is
  // embedded on an unrelated site should not affect if partitioned state is
  // allowed.
  settings.set_content_settings(
      {CreateSetting(kOtherURL, kUnrelatedURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          net::CookieSettingOverrides()));

  // No state is allowed if there's a site-specific rule that blocks access,
  // regardless of the kind of request.
  settings.set_content_settings(
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  // Third-party requests:
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          net::CookieSettingOverrides()));

  // Requests with a null site_for_cookies, but matching top_frame_origin.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kURL)),
                                          net::CookieSettingOverrides()));
  // First-party requests.
  EXPECT_EQ(
      net::NetworkDelegate::PrivacySetting::kStateDisallowed,
      settings.IsPrivacyModeEnabled(
          GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
          url::Origin::Create(GURL(kURL)), net::CookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, IsCookieAccessible) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  // Third-party cookies are blocked, so the cookie should not be accessible by
  // default in a third-party context.
  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalCookie("name", kURL);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // The kForceThirdPartyByUser override can overrule the
  // third-party-cookie-blocking setting.
  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)),
      net::CookieSettingOverrides(
          net::CookieSettingOverride::kForceThirdPartyByUser)));

  settings.set_content_settings(
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});

  // The kForceThirdPartyByUser override can't overrule a site-specific setting.
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)),
      net::CookieSettingOverrides(
          net::CookieSettingOverride::kForceThirdPartyByUser)));

  // The kForceThirdPartyByUser override can't overrule a global setting.
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)),
      net::CookieSettingOverrides(
          net::CookieSettingOverride::kForceThirdPartyByUser)));
}

TEST_P(CookieSettingsTest, IsCookieAccessible_PartitionedCookies) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  std::unique_ptr<net::CanonicalCookie> unpartitioned_cookie =
      MakeCanonicalCookie("unpartitioned", kURL,
                          absl::nullopt /* cookie_partition_key */);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *unpartitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  std::unique_ptr<net::CanonicalCookie> partitioned_cookie =
      MakeCanonicalCookie(
          "__Host-partitioned", kURL,
          net::CookiePartitionKey::FromURLForTesting(GURL(kOtherURL)));

  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // If third-party cookie blocking is disabled, the partitioned cookie should
  // still be available
  settings.set_block_third_party_cookies(false);
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // If there is a site-specific content setting blocking cookies, then
  // partitioned cookies should not be available.
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // If third-party cookie blocking is enabled and there is a site-specific
  // content setting blocking the top-frame origin's own cookies, then
  // the partitioned cookie should not be allowed.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      {CreateSetting(kOtherURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // If third-party cookie blocking is enabled and there is a site-specific
  // setting for the top-frame origin that only applies on an unrelated site,
  // then the partitioned cookie should still be allowed.
  settings.set_content_settings(
      {CreateSetting(kOtherURL, kUnrelatedURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));

  // If third-party cookie blocking is enabled and there is a matching Storage
  // Access setting whose value is BLOCK, then the partitioned cookie should
  // still be allowed.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_ALLOW)});
  settings.set_storage_access_grants(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::CookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, AnnotateAndMoveUserBlockedCookies) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party", kOtherURL), {}},
      {*MakeCanonicalCookie("first_party", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalCookie("excluded_other", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};
  url::Origin origin = url::Origin::Create(GURL(kURL));

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(net::SamePartyContext(),
                                 /*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("first_party"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_other"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY,
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("third_party"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _))));
}

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party_but_member", kFPSMemberURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};

  url::Origin origin = url::Origin::Create(GURL(kFPSOwnerURL));
  net::SchemefulSite primary((GURL(kFPSOwnerURL)));

  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          absl::nullopt);

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kFPSMemberURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(net::SamePartyContext(), &frame_entry,
                                 &top_frame_entry),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_EQ(0u, maybe_included_cookies.size());

  EXPECT_THAT(
      excluded_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("third_party_but_member"),
          MatchesCookieAccessResult(
              HasExactlyExclusionReasonsForTesting(
                  std::vector<net::CookieInclusionStatus::ExclusionReason>{
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
                      net::CookieInclusionStatus::
                          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                  }),
              _, _, _))));
}

TEST_P(
    CookieSettingsTest,
    AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet_FirstPartyURLBlocked) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kFPSOwnerURL, kFPSOwnerURL, CONTENT_SETTING_BLOCK)});

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalCookie("third_party_but_member", kFPSMemberURL);

  url::Origin top_frame_origin = url::Origin::Create(GURL(kFPSOwnerURL));

  // Without third-party-cookie-blocking enabled, the cookie is accessible, even
  // though cookies are blocked for the top-level URL.
  ASSERT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kFPSMemberURL), net::SiteForCookies(), top_frame_origin,
      net::CookieSettingOverrides()));

  // Now we enable third-party-cookie-blocking, and verify that the right
  // exclusion reasons are still applied.
  settings.set_block_third_party_cookies(true);

  net::CookieAccessResultList maybe_included_cookies = {{*cookie, {}}};
  net::CookieAccessResultList excluded_cookies = {};

  net::SchemefulSite primary((GURL(kFPSOwnerURL)));
  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          absl::nullopt);

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kFPSMemberURL), net::SiteForCookies(), &top_frame_origin,
      net::FirstPartySetMetadata(net::SamePartyContext(), &frame_entry,
                                 &top_frame_entry),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_EQ(0u, maybe_included_cookies.size());

  EXPECT_THAT(
      excluded_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("third_party_but_member"),
          MatchesCookieAccessResult(
              HasExactlyExclusionReasonsForTesting(
                  std::vector<net::CookieInclusionStatus::ExclusionReason>{
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
                      net::CookieInclusionStatus::
                          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                  }),
              _, _, _))));
}

namespace {

net::CookieAccessResultList MakeUnpartitionedAndPartitionedCookies() {
  return {
      {*MakeCanonicalCookie("unpartitioned", kURL), {}},
      {*MakeCanonicalCookie(
           "__Host-partitioned", kURL,
           net::CookiePartitionKey::FromURLForTesting(GURL(kOtherURL))),
       {}},
  };
}

}  // namespace

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_PartitionedCookies) {
  CookieSettings settings;

  net::CookieAccessResultList maybe_included_cookies =
      MakeUnpartitionedAndPartitionedCookies();
  net::CookieAccessResultList excluded_cookies = {};

  url::Origin top_level_origin = url::Origin::Create(GURL(kOtherURL));

  // If 3PC blocking is enabled and there are no site-specific content settings
  // then partitioned cookies should be allowed.
  settings.set_block_third_party_cookies(true);
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(net::SamePartyContext(),
                                 /*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(excluded_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("unpartitioned"),
                  MatchesCookieAccessResult(
                      net::HasExclusionReason(
                          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                      _, _, _))));

  // If there is a site-specific content setting blocking cookies, then
  // partitioned cookies should not be allowed.
  maybe_included_cookies = MakeUnpartitionedAndPartitionedCookies();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(net::SamePartyContext(),
                                 /*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("__Host-partitioned"),
              MatchesCookieAccessResult(
                  net::HasExclusionReason(
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("unpartitioned"),
              MatchesCookieAccessResult(
                  net::HasExclusionReason(
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                  _, _, _))));

  // If there is a site-specific content setting blocking cookies on the
  // current top-level origin, then partitioned cookies should not be allowed.
  maybe_included_cookies = MakeUnpartitionedAndPartitionedCookies();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      {CreateSetting(kOtherURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(net::SamePartyContext(),
                                 /*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("__Host-partitioned"),
              MatchesCookieAccessResult(
                  net::HasExclusionReason(
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("unpartitioned"),
              MatchesCookieAccessResult(
                  net::HasExclusionReason(
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                  _, _, _))));

  // If there is a site-specific content setting blocking cookies on the
  // current top-level origin but only when it is embedded on an unrelated site,
  // then partitioned cookies should still be allowed.
  maybe_included_cookies = MakeUnpartitionedAndPartitionedCookies();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      {CreateSetting(kOtherURL, kUnrelatedURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(net::SamePartyContext(),
                                 /*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(excluded_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("unpartitioned"),
                  MatchesCookieAccessResult(
                      net::HasExclusionReason(
                          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                      _, _, _))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTest,
    testing::ValuesIn<TestCase>({
        {"disable_all", /*storage_access_grant_eligible=*/false,
         /*top_level_storage_access_grant_eligible=*/false,
         /*force_allow_third_party_cookies=*/false},
        {"force_3PCs", /*storage_access_grant_eligible=*/false,
         /*top_level_storage_access_grant_eligible=*/false,
         /*force_allow_third_party_cookies=*/true},
        {"enable_TopLevel", /*storage_access_grant_eligible=*/false,
         /*top_level_storage_access_grant_eligible=*/true,
         /*force_allow_third_party_cookies=*/false},
        {"enable_TopLevel_force_3PCs", /*storage_access_grant_eligible=*/false,
         /*top_level_storage_access_grant_eligible=*/true,
         /*force_allow_third_party_cookies=*/true},
        {"enable_StorageAccess", /*storage_access_grant_eligible=*/true,
         /*top_level_storage_access_grant_eligible=*/false,
         /*force_allow_third_party_cookies=*/false},
        {"enable_StorageAccess_force_3PCs",
         /*storage_access_grant_eligible=*/true,
         /*top_level_storage_access_grant_eligible=*/false,
         /*force_allow_third_party_cookies=*/true},
        {"enable_StorageAccess_enable_TopLevel",
         /*storage_access_grant_eligible=*/true,
         /*top_level_storage_access_grant_eligible=*/true,
         /*force_allow_third_party_cookies=*/false},
        {"enable_all", /*storage_access_grant_eligible=*/true,
         /*top_level_storage_access_grant_eligible=*/true,
         /*force_allow_third_party_cookies=*/true},
    }),
    [](const testing::TestParamInfo<CookieSettingsTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace network
