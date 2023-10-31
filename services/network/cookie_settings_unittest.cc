// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

#include "services/network/cookie_settings.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/base/features.h"
#include "net/base/network_delegate.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {
namespace {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::UnorderedElementsAre;

constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";

constexpr char kStorageAccessInputStateHistogram[] =
    "API.StorageAccess.InputState";

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
  // Whether `net::features::kTpcdSupportSettings` is enabled.
  bool eligible_for_3pcd_support;
};

class CookieSettingsTest
    : public testing::TestWithParam<std::tuple<bool, bool, TestCase>> {
 public:
  CookieSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsForceThirdPartyCookieBlockingFlagEnabled()) {
      enabled_features.push_back(net::features::kForceThirdPartyCookieBlocking);
      enabled_features.push_back(net::features::kThirdPartyStoragePartitioning);
    }

    if (Is3pcdSupportEligible()) {
      enabled_features.push_back(net::features::kTpcdSupportSettings);
    } else {
      disabled_features.push_back(net::features::kTpcdSupportSettings);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ContentSettingPatternSource CreateSetting(
      const std::string& primary_pattern,
      const std::string& secondary_pattern,
      ContentSetting setting,
      base::Time expiration = base::Time()) {
    content_settings::RuleMetaData metadata;
    metadata.SetExpirationAndLifetime(
        expiration, expiration.is_null() ? base::TimeDelta()
                                         : expiration - base::Time::Now());
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromString(primary_pattern),
        ContentSettingsPattern::FromString(secondary_pattern),
        base::Value(setting), std::string(), false /* incognito */, metadata);
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  // Indicates whether the setting comes from the testing flag if the test case
  // has 3pc blocked.
  bool IsForceThirdPartyCookieBlockingFlagEnabled() const {
    return std::get<0>(GetParam());
  }

  bool IsTrackingProtectionEnabledFor3pcd() const {
    return std::get<1>(GetParam());
  }

  bool IsStorageAccessGrantEligible() const {
    return std::get<2>(GetParam()).storage_access_grant_eligible;
  }

  bool IsTopLevelStorageAccessGrantEligible() const {
    return std::get<2>(GetParam()).top_level_storage_access_grant_eligible;
  }

  bool Is3pcdSupportEligible() const {
    return std::get<2>(GetParam()).eligible_for_3pcd_support;
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
    return overrides;
  }

  // Assumes that cookie access would be blocked if not for a Storage Access API
  // grant. The `allow` parameter indicates the setting to be returned if cookie
  // access is expected to be allowed.
  ContentSetting SettingWithSaaOverride(ContentSetting allow) const {
    DCHECK(allow == CONTENT_SETTING_ALLOW ||
           allow == CONTENT_SETTING_SESSION_ONLY);
    return IsStorageAccessGrantEligible() ? allow : CONTENT_SETTING_BLOCK;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant.
  ContentSetting SettingWithTopLevelSaaOverride() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    return IsTopLevelStorageAccessGrantEligible() ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK;
  }

  // Assumes that cookie access would be blocked if not for a
  // `ContentSettingsType::TPCD_SUPPORT` setting.
  ContentSetting SettingWith3pcdSupportSetting() const {
    return Is3pcdSupportEligible() ? CONTENT_SETTING_ALLOW
                                   : CONTENT_SETTING_BLOCK;
  }

  // The cookie access result would be blocked if not for a
  // `ContentSettingsType::TPCD_SUPPORT` setting.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWith3pcdSupportSetting() const {
    if (Is3pcdSupportEligible()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The cookie access result would be blocked if not for a Storage Access API
  // grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithSaaOverride() const {
    if (IsStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevelSaaOverride() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    if (IsTopLevelStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
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
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), GURL(kURL),
                                      GetCookieSettingOverrides(), nullptr),
            IsForceThirdPartyCookieBlockingFlagEnabled()
                ? CONTENT_SETTING_BLOCK
                : CONTENT_SETTING_ALLOW);

  // This is blocked because both patterns match.
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
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
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED, 1);
}

TEST_P(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest,
       GetCookieSettingOverridePreservesSessionOnlySetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_SESSION_ONLY));
}

TEST_P(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
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
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
      1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(kAllowedRequestsHistogram,
                                     net::cookie_util::StorageAccessResult::
                                         ACCESS_ALLOWED_STORAGE_ACCESS_GRANT,
                                     IsStorageAccessGrantEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
      IsStorageAccessGrantEligible() ? 1 : 2);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectBucketCount(
      kStorageAccessInputStateHistogram,
      IsStorageAccessGrantEligible()
          ? net::cookie_util::StorageAccessInputState::kOptInWithoutGrant
          : net::cookie_util::StorageAccessInputState::kNoOptInNoGrant,
      3);

  // If third-party cookies are blocked, SAA grant takes precedence over
  // possible override to allow 3PCs.
  {
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              SettingWithSaaOverride(CONTENT_SETTING_ALLOW));
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
        1);

    histogram_tester_2.ExpectUniqueSample(
        kStorageAccessInputStateHistogram,
        IsStorageAccessGrantEligible()
            ? net::cookie_util::StorageAccessInputState::kOptInWithGrant
            : net::cookie_util::StorageAccessInputState::kGrantWithoutOptIn,
        1);
  }

  // If cookies are globally blocked, SAA grants and 3PC override
  // should both be ignored.
  {
    settings.set_content_settings(
        ContentSettingsType::COOKIES,
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
    histogram_tester_2.ExpectTotalCount(kStorageAccessInputStateHistogram, 0);
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
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  // Only set the storage access granted by Top-Level Storage Access API.
  settings.set_content_settings(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevelSaaOverride());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevelSaaOverride(), 1);

  // Check the cookie setting that does not match the top-level storage access
  // grant--the |top_level_url| granting access to |url| is now being loaded
  // under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  // TODO(crbug.com/1385156): Separate metrics between StorageAccessAPI
  // and the page-level variant.
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT,
      IsTopLevelStorageAccessGrantEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevelSaaOverride(),
      IsTopLevelStorageAccessGrantEligible() ? 1 : 2);

  // Check the cookie setting that does not match the top-level storage access
  // grant where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // If third-party cookies are blocked, Top-Level Storage Access grant takes
  // precedence over possible override to allow third-party cookies.
  {
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              SettingWithTopLevelSaaOverride());
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        BlockedStorageAccessResultWithTopLevelSaaOverride(), 1);
  }

  // If cookies are globally blocked, Top-Level Storage Access grants and 3PC
  // override should both be ignored.
  {
    settings.set_content_settings(
        ContentSettingsType::COOKIES,
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
  }
}

// Subdomains of the granted embedding url should not gain access if a valid
// grant exists.
TEST_P(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kDomainURL);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kDomain, top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(settings.GetCookieSetting(GURL(kSubDomainURL), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_P(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  GURL top_level_url = GURL(kDomainURL);
  GURL url = GURL(kURL);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(url.host(), kDomain, CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(settings.GetCookieSetting(url, GURL(kSubDomainURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_P(CookieSettingsTest, GetCookieSettingSAARespectsSettings) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  base::HistogramTester histogram_tester;

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectTotalCount(kStorageAccessInputStateHistogram, 0);
}

// Once a grant expires access should no longer be given.
TEST_P(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  base::Time expiration_time = base::Time::Now() + base::Seconds(100);
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW,
                     expiration_time)});

  base::HistogramTester histogram_tester;
  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));
  histogram_tester.ExpectUniqueSample(
      kStorageAccessInputStateHistogram,
      IsStorageAccessGrantEligible()
          ? net::cookie_util::StorageAccessInputState::kOptInWithGrant
          : net::cookie_util::StorageAccessInputState::kGrantWithoutOptIn,
      1);

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectBucketCount(
      kStorageAccessInputStateHistogram,
      IsStorageAccessGrantEligible()
          ? net::cookie_util::StorageAccessInputState::kOptInWithoutGrant
          : net::cookie_util::StorageAccessInputState::kNoOptInNoGrant,
      1);
}

TEST_P(CookieSettingsTest, GetCookieSetting3pcdUnblocks) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  GURL third_url = GURL(kDomainURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TPCD_SUPPORT,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWith3pcdSupportSetting());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdSupportSetting(), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD,
      Is3pcdSupportEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdSupportSetting(),
      Is3pcdSupportEligible() ? 1 : 2);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings.GetCookieSetting(third_url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // If third-party cookies are blocked, 3PCD settings take precedence over
  // possible override to allow 3PCs.
  {
    settings.set_block_third_party_cookies(true);
    settings.set_mitigations_enabled_for_3pcd(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              SettingWith3pcdSupportSetting());
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        BlockedStorageAccessResultWith3pcdSupportSetting(), 1);
  }

  // If cookies are globally blocked, the 3PC overrides should both be ignored.
  {
    settings.set_content_settings(
        ContentSettingsType::COOKIES,
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    settings.set_block_third_party_cookies(true);
    settings.set_mitigations_enabled_for_3pcd(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                        GetCookieSettingOverrides(), nullptr),
              CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
  }
}

// Any 3PCD setting should not override an explicit setting to block cookie
// access.
TEST_P(CookieSettingsTest, GetCookieSetting3pcdRespectsSettings) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_content_settings(
      ContentSettingsType::TPCD_SUPPORT,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  base::HistogramTester histogram_tester;

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectTotalCount(kStorageAccessInputStateHistogram, 0);
}

// A 3PCD setting should not affect cookie settings when the
// net::features::kTpcdSupportSettings feature is disabled.
TEST_P(CookieSettingsTest, GetCookieSetting3pcdFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kTpcdSupportSettings);

  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TPCD_SUPPORT,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_P(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST_P(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
  CookieSettings settings;
  settings.set_secure_origin_cookies_allowed_schemes({"chrome"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("https://foo.com") /* url */,
                                GURL("chrome://foo") /* first_party_url */,
                                GetCookieSettingOverrides()),
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome://foo") /* url */,
                                GURL("https://foo.com") /* first_party_url */,
                                GetCookieSettingOverrides()),
      CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("http://foo.com") /* url */,
                                GURL("chrome://foo") /* first_party_url */,
                                GetCookieSettingOverrides()),
      CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
  CookieSettings settings;
  settings.set_third_party_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides()),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("other-scheme://foo") /* first_party_url */,
                GetCookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                                GURL("http://foo.com") /* first_party_url */,
                                GetCookieSettingOverrides()),
      CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
  CookieSettings settings;
  settings.set_matching_scheme_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://bar") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides()),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */,
                GURL("chrome-extension://foo") /* first_party_url */,
                GetCookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(
      settings.GetCookieSetting(GURL("chrome-extension://foo") /* url */,
                                GURL("http://foo.com") /* first_party_url */,
                                GetCookieSettingOverrides()),
      CONTENT_SETTING_BLOCK);
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
  settings.set_content_settings(
      ContentSettingsType::LEGACY_COOKIE_ACCESS,
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
  settings.set_content_settings(
      ContentSettingsType::LEGACY_COOKIE_ACCESS,
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

  const net::NetworkDelegate::PrivacySetting
      third_party_partitioned_or_all_allowed =
          net::NetworkDelegate::PrivacySetting::kPartitionedStateAllowedOnly;

  // Third-party requests should only have accessed to partitioned state.
  EXPECT_EQ(third_party_partitioned_or_all_allowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          GetCookieSettingOverrides()));

  // Same for requests with a null site_for_cookies, even if the
  // top_frame_origin matches.
  const net::NetworkDelegate::PrivacySetting
      first_party_partitioned_or_all_allowed =
          IsStorageAccessGrantEligible()
              ? net::NetworkDelegate::PrivacySetting::kStateAllowed
              : net::NetworkDelegate::PrivacySetting::
                    kPartitionedStateAllowedOnly;
  EXPECT_EQ(first_party_partitioned_or_all_allowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kURL)),
                                          GetCookieSettingOverrides()));

  // The first party is able to send any type of state.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateAllowed,
            settings.IsPrivacyModeEnabled(
                GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
                url::Origin::Create(GURL(kURL)), GetCookieSettingOverrides()));

  // Setting a site-specific rule for the top-level frame origin when it is
  // embedded on an unrelated site should not affect if partitioned state is
  // allowed.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kOtherURL, kUnrelatedURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(third_party_partitioned_or_all_allowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          GetCookieSettingOverrides()));

  // No state is allowed if there's a site-specific rule that blocks access,
  // regardless of the kind of request.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  // Third-party requests:
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kOtherURL)),
                                          GetCookieSettingOverrides()));

  // Requests with a null site_for_cookies, but matching top_frame_origin.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(GURL(kURL), net::SiteForCookies(),
                                          url::Origin::Create(GURL(kURL)),
                                          GetCookieSettingOverrides()));
  // First-party requests.
  EXPECT_EQ(net::NetworkDelegate::PrivacySetting::kStateDisallowed,
            settings.IsPrivacyModeEnabled(
                GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
                url::Origin::Create(GURL(kURL)), GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, IsCookieAccessible) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalCookie("name", kURL);

  EXPECT_TRUE(
      settings.IsCookieAccessible(*cookie, GURL(kURL), net::SiteForCookies(),
                                  url::Origin::Create(GURL(kOtherURL)),
                                  GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));

  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  // Third-party cookies are blocked, so the cookie should not be accessible by
  // default in a third-party context.
  status.ResetForTesting();
  EXPECT_FALSE(
      settings.IsCookieAccessible(*cookie, GURL(kURL), net::SiteForCookies(),
                                  url::Origin::Create(GURL(kOtherURL)),
                                  GetCookieSettingOverrides(), &status));
  EXPECT_FALSE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));
  EXPECT_TRUE(status.HasExclusionReason(
      IsForceThirdPartyCookieBlockingFlagEnabled() ||
              IsTrackingProtectionEnabledFor3pcd()
          ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
          : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));

  // Note that the SiteForCookies matches nothing, so this is a third-party
  // context even though the `url` matches the `top_frame_origin`.
  status.ResetForTesting();
  EXPECT_EQ(
      settings.IsCookieAccessible(*cookie, GURL(kURL), net::SiteForCookies(),
                                  url::Origin::Create(GURL(kURL)),
                                  GetCookieSettingOverrides(), &status),
      IsStorageAccessGrantEligible());
  EXPECT_THAT(status.HasExclusionReason(
                  IsForceThirdPartyCookieBlockingFlagEnabled() ||
                          IsTrackingProtectionEnabledFor3pcd()
                      ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
                      : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
              Not(IsStorageAccessGrantEligible()));

  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});

  // No override can overrule a site-specific setting.
  status.ResetForTesting();
  EXPECT_FALSE(
      settings.IsCookieAccessible(*cookie, GURL(kURL), net::SiteForCookies(),
                                  url::Origin::Create(GURL(kOtherURL)),
                                  GetCookieSettingOverrides(), &status));
  // Cookies blocked by a site-specific setting should still use
  // `EXCLUDE_USER_PREFERENCES` reason.
  EXPECT_TRUE(status.HasExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));

  // No override can overrule a global setting.
  status.ResetForTesting();
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(
      settings.IsCookieAccessible(*cookie, GURL(kURL), net::SiteForCookies(),
                                  url::Origin::Create(GURL(kOtherURL)),
                                  GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.HasExclusionReason(
      IsForceThirdPartyCookieBlockingFlagEnabled() ||
              IsTrackingProtectionEnabledFor3pcd()
          ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
          : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));
}

TEST_P(CookieSettingsTest, IsCookieAccessible_PartitionedCookies) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  std::unique_ptr<net::CanonicalCookie> partitioned_cookie =
      MakeCanonicalCookie(
          "__Host-partitioned", kURL,
          net::CookiePartitionKey::FromURLForTesting(GURL(kOtherURL)));

  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // If third-party cookie blocking is disabled, the partitioned cookie should
  // still be available
  settings.set_block_third_party_cookies(false);
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // If there is a site-specific content setting blocking cookies, then
  // partitioned cookies should not be available.
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // If third-party cookie blocking is enabled and there is a site-specific
  // content setting blocking the top-frame origin's cookies, then the
  // partitioned cookie should be blocked as well.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // If third-party cookie blocking is enabled and there is a site-specific
  // setting for the top-frame origin that only applies on an unrelated site,
  // then the partitioned cookie should still be allowed.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kUnrelatedURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // If third-party cookie blocking is enabled and there is a matching Storage
  // Access setting whose value is BLOCK, then the partitioned cookie should
  // still be allowed.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_ALLOW)});
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), GetCookieSettingOverrides(),
      &status));

  // Partitioned cookies are not affected by 3pc phaseout, so the
  // *_THIRD_PARTY_PHASEOUT warning/exclusion reason is irrelevant.
  EXPECT_FALSE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));
  EXPECT_FALSE(status.HasExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT));
}

TEST_P(CookieSettingsTest, AnnotateAndMoveUserBlockedCookies_CrossSiteEmbed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party", kURL), {}},
      {*MakeCanonicalCookie("cookie", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalCookie("excluded_other", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};
  url::Origin origin = url::Origin::Create(GURL(kOtherURL));

  const bool expected_any_allowed = false;

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(
                    /*frame_entry=*/nullptr,
                    /*top_frame_entry=*/nullptr),
                GetCookieSettingOverrides(), maybe_included_cookies,
                excluded_cookies),
            expected_any_allowed);

  if (expected_any_allowed) {
    EXPECT_THAT(
        maybe_included_cookies,
        ElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    AllOf(net::IsInclude(),
                          Not(net::HasWarningReason(
                              net::CookieInclusionStatus::WarningReason::
                                  WARN_THIRD_PARTY_PHASEOUT))),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("cookie"),
                MatchesCookieAccessResult(
                    AllOf(net::IsInclude(),
                          Not(net::HasWarningReason(
                              net::CookieInclusionStatus::WarningReason::
                                  WARN_THIRD_PARTY_PHASEOUT))),
                    _, _, _))));
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("excluded_other"),
            MatchesCookieAccessResult(
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        net::CookieInclusionStatus::ExclusionReason::
                            EXCLUDE_SECURE_ONLY}),
                _, _, _))));
  } else {
    EXPECT_THAT(maybe_included_cookies, IsEmpty());
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("cookie"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                    IsTrackingProtectionEnabledFor3pcd()
                                ? net::CookieInclusionStatus::
                                      EXCLUDE_THIRD_PARTY_PHASEOUT
                                : net::CookieInclusionStatus::
                                      EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY,
                            IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                    IsTrackingProtectionEnabledFor3pcd()
                                ? net::CookieInclusionStatus::
                                      EXCLUDE_THIRD_PARTY_PHASEOUT
                                : net::CookieInclusionStatus::
                                      EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                    IsTrackingProtectionEnabledFor3pcd()
                                ? net::CookieInclusionStatus::
                                      EXCLUDE_THIRD_PARTY_PHASEOUT
                                : net::CookieInclusionStatus::
                                      EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_CrossSiteEmbed_3PCAllowed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(false);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};
  url::Origin origin = url::Origin::Create(GURL(kOtherURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  // Verify that the allowed cookie has the expected warning reason.
  EXPECT_THAT(
      maybe_included_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("third_party"),
          MatchesCookieAccessResult(
              AllOf(net::IsInclude(),
                    net::HasExactlyWarningReasonsForTesting(
                        std::vector<net::CookieInclusionStatus::WarningReason>{
                            net::CookieInclusionStatus::WarningReason::
                                WARN_THIRD_PARTY_PHASEOUT})),
              _, _, _))));
}

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SameSiteEmbed_3PCAllowed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(false);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("cookie", kDomainURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};
  url::Origin origin = url::Origin::Create(GURL(kDomainURL));

  // This is a first-party context.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kSubDomainURL), net::SiteForCookies(net::SchemefulSite(origin)),
      &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  // Verify that the allowed cookie does not have the 3PC warning reason.
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("cookie"),
                  MatchesCookieAccessResult(
                      AllOf(net::IsInclude(),
                            Not(net::HasWarningReason(
                                net::CookieInclusionStatus::WarningReason::
                                    WARN_THIRD_PARTY_PHASEOUT))),
                      _, _, _))));

  // This is a third-party context, even though the request URL and the
  // top-frame URL are same-site with each other.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kSubDomainURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  // Verify that the allowed cookie has the 3PC warning reason.
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("cookie"),
                  MatchesCookieAccessResult(
                      AllOf(net::IsInclude(),
                            net::HasWarningReason(
                                net::CookieInclusionStatus::WarningReason::
                                    WARN_THIRD_PARTY_PHASEOUT)),
                      _, _, _))));
}

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SameSiteEmbed_ThirdPartyContext) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("cookie", kDomainURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalCookie("excluded_other", kDomainURL),
       // The ExclusionReason below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};
  url::Origin origin = url::Origin::Create(GURL(kDomainURL));

  const bool expected_any_allowed = IsStorageAccessGrantEligible();

  // Note that the site of `url` matches the site of `top_frame_origin`. This is
  // a third-party context for the purposes of third-party-cookie-blocking, even
  // though the request URL and the top-frame URL are same-site with each other.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kSubDomainURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(
                    /*frame_entry=*/nullptr,
                    /*top_frame_entry=*/nullptr),
                GetCookieSettingOverrides(), maybe_included_cookies,
                excluded_cookies),
            expected_any_allowed);

  if (expected_any_allowed) {
    EXPECT_THAT(maybe_included_cookies,
                ElementsAre(MatchesCookieWithAccessResult(
                    net::MatchesCookieWithName("cookie"),
                    MatchesCookieAccessResult(
                        AllOf(net::IsInclude(),
                              Not(net::HasWarningReason(
                                  net::CookieInclusionStatus::WarningReason::
                                      WARN_THIRD_PARTY_PHASEOUT))),
                        _, _, _))));
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("excluded_other"),
            MatchesCookieAccessResult(
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        net::CookieInclusionStatus::ExclusionReason::
                            EXCLUDE_SECURE_ONLY}),
                _, _, _))));
  } else {
    EXPECT_THAT(maybe_included_cookies, IsEmpty());
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("cookie"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                    IsTrackingProtectionEnabledFor3pcd()
                                ? net::CookieInclusionStatus::
                                      EXCLUDE_THIRD_PARTY_PHASEOUT
                                : net::CookieInclusionStatus::
                                      EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY,
                            IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                    IsTrackingProtectionEnabledFor3pcd()
                                ? net::CookieInclusionStatus::
                                      EXCLUDE_THIRD_PARTY_PHASEOUT
                                : net::CookieInclusionStatus::
                                      EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("third_party_but_member", kFPSMemberURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};

  url::Origin origin = url::Origin::Create(GURL(kFPSOwnerURL));
  net::SchemefulSite primary((GURL(kFPSOwnerURL)));

  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          absl::nullopt);

  const bool expected_allowed = false;

  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kFPSMemberURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
                GetCookieSettingOverrides(), maybe_included_cookies,
                excluded_cookies),
            expected_allowed);

  if (expected_allowed) {
    EXPECT_EQ(0u, excluded_cookies.size());
    EXPECT_THAT(
        maybe_included_cookies,
        ElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("third_party_but_member"),
            MatchesCookieAccessResult(
                AllOf(net::IsInclude(), Not(net::HasWarningReason(
                                            net::CookieInclusionStatus::
                                                WARN_THIRD_PARTY_PHASEOUT))),
                _, _, _))));
  } else {
    EXPECT_EQ(0u, maybe_included_cookies.size());
    EXPECT_THAT(
        excluded_cookies,
        ElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("third_party_but_member"),
            MatchesCookieAccessResult(
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                IsTrackingProtectionEnabledFor3pcd()
                            ? net::CookieInclusionStatus::
                                  EXCLUDE_THIRD_PARTY_PHASEOUT
                            : net::CookieInclusionStatus::
                                  EXCLUDE_USER_PREFERENCES,
                        net::CookieInclusionStatus::
                            EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                    }),
                _, _, _))));
  }
}

TEST_P(
    CookieSettingsTest,
    AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet_FirstPartyURLBlocked) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kFPSOwnerURL, kFPSOwnerURL, CONTENT_SETTING_BLOCK)});

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalCookie("third_party_but_member", kFPSMemberURL);

  url::Origin top_frame_origin = url::Origin::Create(GURL(kFPSOwnerURL));

  // Without third-party-cookie-blocking enabled, the cookie is accessible, even
  // though cookies are blocked for the top-level URL.
  ASSERT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kFPSMemberURL), net::SiteForCookies(), top_frame_origin,
      GetCookieSettingOverrides(), &status));

  EXPECT_TRUE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));

  // Now we enable third-party-cookie-blocking, and verify that the right
  // exclusion reasons are still applied.
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {{*cookie, {}}};
  net::CookieAccessResultList excluded_cookies = {};

  net::SchemefulSite primary((GURL(kFPSOwnerURL)));
  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          absl::nullopt);

  const bool expected_allowed = false;

  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kFPSMemberURL), net::SiteForCookies(), &top_frame_origin,
                net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
                GetCookieSettingOverrides(), maybe_included_cookies,
                excluded_cookies),
            expected_allowed);

  if (expected_allowed) {
    EXPECT_EQ(0u, excluded_cookies.size());

    EXPECT_THAT(
        maybe_included_cookies,
        ElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("third_party_but_member"),
            MatchesCookieAccessResult(
                AllOf(net::IsInclude(), Not(net::HasWarningReason(
                                            net::CookieInclusionStatus::
                                                WARN_THIRD_PARTY_PHASEOUT))),
                _, _, _))));
  } else {
    EXPECT_EQ(0u, maybe_included_cookies.size());

    EXPECT_THAT(
        excluded_cookies,
        ElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("third_party_but_member"),
            MatchesCookieAccessResult(
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        IsForceThirdPartyCookieBlockingFlagEnabled() ||
                                IsTrackingProtectionEnabledFor3pcd()
                            ? net::CookieInclusionStatus::
                                  EXCLUDE_THIRD_PARTY_PHASEOUT
                            : net::CookieInclusionStatus::
                                  EXCLUDE_USER_PREFERENCES,
                        net::CookieInclusionStatus::
                            EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                    }),
                _, _, _))));
  }
}

namespace {

net::CookieAccessResultList MakePartitionedCookie() {
  return {
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

  net::CookieAccessResultList maybe_included_cookies = MakePartitionedCookie();
  net::CookieAccessResultList excluded_cookies = {};

  url::Origin top_level_origin = url::Origin::Create(GURL(kOtherURL));

  // If 3PC blocking is enabled and there are no site-specific content settings
  // then partitioned cookies should be allowed.
  settings.set_block_third_party_cookies(true);
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(excluded_cookies, IsEmpty());

  // If there is a site-specific content setting blocking cookies, then
  // partitioned cookies should not be allowed.
  maybe_included_cookies = MakePartitionedCookie();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(excluded_cookies,
              UnorderedElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(
                      net::HasExclusionReason(
                          net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                      _, _, _))));

  // If there is a site-specific content setting blocking cookies on the
  // current origin, then partitioned cookies should not be allowed.
  maybe_included_cookies = MakePartitionedCookie();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(/*frame_entry=*/nullptr,
                                 /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  {
    EXPECT_THAT(maybe_included_cookies, IsEmpty());
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("__Host-partitioned"),
            MatchesCookieAccessResult(
                net::HasExclusionReason(
                    net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
                _, _, _))));
  }

  // If there is a site-specific content setting blocking cookies on the
  // current top-level origin but only when it is embedded on an unrelated site,
  // then partitioned cookies should still be allowed.
  maybe_included_cookies = MakePartitionedCookie();
  excluded_cookies = {};
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kOtherURL, kUnrelatedURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &top_level_origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(excluded_cookies, IsEmpty());
}

const TestCase kOverrideTestCases[] = {
    {"disable_all", /*storage_access_grant_eligible=*/false,
     /*top_level_storage_access_grant_eligible=*/false,
     /*eligible_for_3pcd_support=*/false},
    {"disable_SAA_disable_TopLevel_enable_3PCD",
     /*storage_access_grant_eligible=*/false,
     /*top_level_storage_access_grant_eligible=*/false,
     /*eligible_for_3pcd_support=*/true},
    {"disable_SAA_enable_TopLevel_disable_3PCD",
     /*storage_access_grant_eligible=*/false,
     /*top_level_storage_access_grant_eligible=*/true,
     /*eligible_for_3pcd_support=*/false},
    {"disable_SAA_enable_TopLevel_enable_3PCD",
     /*storage_access_grant_eligible=*/false,
     /*top_level_storage_access_grant_eligible=*/true,
     /*eligible_for_3pcd_support=*/true},
    {"enable_SAA_disable_TopLevel_disable_3PCD",
     /*storage_access_grant_eligible=*/true,
     /*top_level_storage_access_grant_eligible=*/false,
     /*eligible_for_3pcd_support=*/false},
    {"enable_SAA_disable_TopLevel_enable_3PCD",
     /*storage_access_grant_eligible=*/true,
     /*top_level_storage_access_grant_eligible=*/false,
     /*eligible_for_3pcd_support=*/true},
    {"enable_SAA_enable_TopLevel_disable_3PCD",
     /*storage_access_grant_eligible=*/true,
     /*top_level_storage_access_grant_eligible=*/true,
     /*eligible_for_3pcd_support=*/false},
    {"enable_all", /*storage_access_grant_eligible=*/true,
     /*top_level_storage_access_grant_eligible=*/true,
     /*eligible_for_3pcd_support=*/true},
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::ValuesIn(kOverrideTestCases)),
    // Print test name. `info` is type of std::tuple<bool, TestCase>.
    [](const testing::TestParamInfo<CookieSettingsTest::ParamType>& info) {
      std::stringstream ss;
      ss << (std::get<0>(info.param) ? "testing_3pcb_on" : "testing_3pcb_off")
         << "_AND_"
         << (std::get<1>(info.param) ? "tracking_protection_3pcb_on"
                                     : "tracking_protection_3pcb_off")
         << "_AND_" << std::get<2>(info.param).test_name;
      std::string s = ss.str();
      return ss.str();
    });

}  // namespace
}  // namespace network
