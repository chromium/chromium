// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
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
#include "net/cookies/static_cookie_policy.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/tpcd/metadata/manager.h"
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

constexpr char kDomainURL[] = "http://example.com";
constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";
constexpr char kSubDomainURL[] = "http://www.corp.example.com";
constexpr char kDomain[] = "example.com";
constexpr char kDotDomain[] = ".example.com";
constexpr char kSubDomain[] = "www.corp.example.com";
constexpr char kOtherDomain[] = "not-example.com";
constexpr char kDomainWildcardPattern[] = "[*.]example.com";
constexpr char kRwsOwnerURL[] = "https://rws-owner.test";
constexpr char kRwsMemberURL[] = "https://rws-member.test";
constexpr char kUnrelatedURL[] = "http://unrelated.com";

std::unique_ptr<net::CanonicalCookie> MakeCanonicalCookie(
    const std::string& name,
    const std::string& domain,
    std::optional<net::CookiePartitionKey> cookie_partition_key =
        std::nullopt) {
  return net::CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "1", domain, /*path=*/"/", /*creation=*/base::Time(),
      /*expiration=*/base::Time(), /*last_access=*/base::Time(),
      /*last_update=*/base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
      net::CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie_partition_key);
}

std::unique_ptr<net::CanonicalCookie> MakeCanonicalSameSiteNoneCookie(
    const std::string& name,
    const std::string& domain,
    std::optional<net::CookiePartitionKey> cookie_partition_key =
        std::nullopt) {
  return net::CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "1", domain, /*path=*/"/", /*creation=*/base::Time(),
      /*expiration=*/base::Time(), /*last_access=*/base::Time(),
      /*last_update=*/base::Time(),
      /*secure=*/true, /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
      net::CookiePriority::COOKIE_PRIORITY_DEFAULT, cookie_partition_key);
}

// NOTE: Consider modifying
// /components/content_settings/core/browser/cookie_settings_unittest.cc if
// applicable.

// To avoid an explosion of test cases, please don't just add a boolean to
// the test features. Consider whether features can interact with each other and
// whether you really need all combinations.

// Controls features that can unblock 3p cookies.
enum GrantSource {
  // Not eligible for additional grants.
  kNoneGranted,
  // Eligible for StorageAccess grants via API usage.
  kStorageAccessGrantsEligibleViaAPI,
  // Can use Storage Access permission grants via an HTTP response header.
  kStorageAccessGrantsEligibleViaHeader,
  // Eligible for TopLevelStorageAccess grants.
  kTopLevelStorageAccessGrantEligible,

  kGrantSourceCount
};

// Features that can block 3p cookies.
enum BlockSource {
  // 3PCD is not enabled. But the test case can still block 3pc.
  kNoneBlocked,
  // Tracking protection enabled by default.
  kTrackingProtectionEnabledFor3pcd,
  // Third-party cookie blocking is enabled through a flag.
  kForceThirdPartyCookieBlockingFlagEnabled,

  kBlockSourceCount

};

enum TestVariables {
  kGrantSource,
  kBlockSource,
};

class CookieSettingsTestBase : public testing::Test {
 public:
  CookieSettingsTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ContentSettingPatternSource CreateSetting(
      const std::string& primary_pattern,
      const std::string& secondary_pattern,
      ContentSetting setting,
      base::Time expiration = base::Time(),
      content_settings::ProviderType source =
          content_settings::ProviderType::kNone,
      bool off_the_record = false) {
    content_settings::RuleMetaData metadata;
    metadata.SetExpirationAndLifetime(
        expiration, expiration.is_null() ? base::TimeDelta()
                                         : expiration - base::Time::Now());
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromString(primary_pattern),
        ContentSettingsPattern::FromString(secondary_pattern),
        base::Value(setting), source, off_the_record, metadata);
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Default test class to be used by most tests. If you want to add a new
// parameter, consider whether all test cases actually require this parameter
// or whether it is sufficient to add a new subclass of CookieSettingsTestBase.
class CookieSettingsTest : public CookieSettingsTestBase {};

// Parameterized class that tests combinations of StorageAccess grants and 3pcd
// grants. Tests that don't need the whole range of combinations should create
// their own parameterized subclasses.
class CookieSettingsTestP : public CookieSettingsTestBase,
                            public testing::WithParamInterface<
                                std::tuple</*kGrantSource*/ GrantSource,
                                           /*kBlockSource*/ BlockSource>> {
 public:
  CookieSettingsTestP() {
    std::vector<base::test::FeatureRef> enabled_features;
    if (IsForceThirdPartyCookieBlockingFlagEnabled()) {
      enabled_features.push_back(net::features::kForceThirdPartyCookieBlocking);
      enabled_features.push_back(net::features::kThirdPartyStoragePartitioning);
    }

    feature_list_.InitWithFeatures(enabled_features, {});
  }

  // Indicates whether the setting comes from the testing flag if the
  // test case has 3pc blocked.
  bool IsForceThirdPartyCookieBlockingFlagEnabled() const {
    return std::get<TestVariables::kBlockSource>(GetParam()) ==
           BlockSource::kForceThirdPartyCookieBlockingFlagEnabled;
  }

  bool IsTrackingProtectionEnabledFor3pcd() const {
    return std::get<TestVariables::kBlockSource>(GetParam()) ==
           BlockSource::kTrackingProtectionEnabledFor3pcd;
  }

  bool IsTPCDEnabled() const {
    return IsForceThirdPartyCookieBlockingFlagEnabled() ||
           IsTrackingProtectionEnabledFor3pcd();
  }

  bool IsStorageAccessGrantEligibleViaAPI() const {
    return std::get<TestVariables::kGrantSource>(GetParam()) ==
           GrantSource::kStorageAccessGrantsEligibleViaAPI;
  }

  bool IsStorageAccessGrantEligibleViaHeader() const {
    return std::get<TestVariables::kGrantSource>(GetParam()) ==
           GrantSource::kStorageAccessGrantsEligibleViaHeader;
  }

  bool IsTopLevelStorageAccessGrantEligible() const {
    return std::get<TestVariables::kGrantSource>(GetParam()) ==
           GrantSource::kTopLevelStorageAccessGrantEligible;
  }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    if (IsStorageAccessGrantEligibleViaAPI()) {
      overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
    }
    if (IsStorageAccessGrantEligibleViaHeader()) {
      overrides.Put(
          net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader);
    }
    if (IsTopLevelStorageAccessGrantEligible()) {
      overrides.Put(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    }
    return overrides;
  }

  // Assumes that cookie access would be blocked if not for a Storage
  // Access API grant. The `allow` parameter indicates the setting to
  // be returned if cookie access is expected to be allowed.
  ContentSetting SettingWithSaaOverride(ContentSetting allow) const {
    DCHECK(allow == CONTENT_SETTING_ALLOW ||
           allow == CONTENT_SETTING_SESSION_ONLY);
    return IsStorageAccessGrantEligibleViaAPI() ||
                   IsStorageAccessGrantEligibleViaHeader()
               ? allow
               : CONTENT_SETTING_BLOCK;
  }

  // Assumes that cookie access would be blocked if not for some usage of the
  // Storage Access API. Note that this is not the same thing as having a SAA
  // permission grant.
  ContentSetting SettingWithSaaViaAPI(ContentSetting allow) const {
    DCHECK(allow == CONTENT_SETTING_ALLOW ||
           allow == CONTENT_SETTING_SESSION_ONLY);
    return IsStorageAccessGrantEligibleViaAPI() ? allow : CONTENT_SETTING_BLOCK;
  }

  // A version of above that considers Top-Level Storage Access API
  // grant instead of Storage Access API grant.
  ContentSetting SettingWithTopLevelSaaOverride() const {
    // TODO(crbug.com/40246640): Check TopLevelStorageAccessAPI
    // instead after separating the feature flag.
    return IsTopLevelStorageAccessGrantEligible() ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK;
  }

  // The cookie access result would be blocked if not for a Storage
  // Access API grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithSaaOverride() const {
    if (IsStorageAccessGrantEligibleViaAPI() ||
        IsStorageAccessGrantEligibleViaHeader()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The cookie access result would be blocked if not for some Storage Access
  // API usage. Note that this is not the same thing as presence of a permission
  // grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithSaaViaAPI() const {
    if (IsStorageAccessGrantEligibleViaAPI()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // A version of above that considers Top-Level Storage Access API
  // grant instead of Storage Access API grant.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevelSaaOverride() const {
    // TODO(crbug.com/40246640): Check TopLevelStorageAccessAPI
    // instead after separating the feature flag.
    if (IsTopLevelStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }
};

TEST_F(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(), GURL(kURL),
                                net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_ALLOW);
}

TEST_F(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(), GURL(kURL),
                                net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
}

class CookieSettingsTrackingProtectionTest : public CookieSettingsTestBase {
 public:
  CookieSettingsTrackingProtectionTest() {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kTrackingProtectionContentSettingFor3pcb);
  }
};

// The TRACKING_PROTECTION content setting is not registered on iOS
#if !BUILDFLAG(IS_IOS)
TEST_F(CookieSettingsTrackingProtectionTest,
       GetCookieSettingUsesTrackingProtectionSetting) {
  // Set default to block 3PCs
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kURL), net::CookieSettingOverrides(),
                                      nullptr),
            CONTENT_SETTING_BLOCK);

  // Add TRACKING_PROTECTION exception
  settings.set_content_settings(
      ContentSettingsType::TRACKING_PROTECTION,
      {CreateSetting("*", kURL, CONTENT_SETTING_ALLOW)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kURL), net::CookieSettingOverrides(),
                                      nullptr),
            CONTENT_SETTING_ALLOW);

  // Explicitly block COOKIES for the URL. This should take priority over the
  // TRACKING_PROTECTION exception
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kOtherURL, kURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kURL), net::CookieSettingOverrides(),
                                      nullptr),
            CONTENT_SETTING_BLOCK);
}
#endif

TEST_F(CookieSettingsTest, GetCookieSettingMultipleProviders) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY, base::Time(),
                     content_settings::ProviderType::kPolicyProvider),
       CreateSetting("*", "*", CONTENT_SETTING_BLOCK, base::Time(),
                     content_settings::ProviderType::kPolicyProvider),
       CreateSetting(kOtherURL, kOtherURL, CONTENT_SETTING_ALLOW, base::Time(),
                     content_settings::ProviderType::kPrefProvider),
       CreateSetting("*", "*", CONTENT_SETTING_ALLOW, base::Time(),
                     content_settings::ProviderType::kDefaultProvider)});
  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(), GURL(kURL),
                                net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingOtrProviders) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY, base::Time(),
                     content_settings::ProviderType::kPrefProvider, true),
       CreateSetting("*", "*", CONTENT_SETTING_BLOCK, base::Time(),
                     content_settings::ProviderType::kPrefProvider, true),
       CreateSetting(kOtherURL, kOtherURL, CONTENT_SETTING_ALLOW, base::Time(),
                     content_settings::ProviderType::kPrefProvider, false),
       CreateSetting("*", "*", CONTENT_SETTING_ALLOW, base::Time(),
                     content_settings::ProviderType::kDefaultProvider, false)});
  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(), GURL(kURL),
                                net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTestP, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_EQ(settings.GetCookieSetting(
                GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)),
                GURL(kURL), GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kURL), GetCookieSettingOverrides(),
                                      nullptr),
            IsForceThirdPartyCookieBlockingFlagEnabled()
                ? CONTENT_SETTING_BLOCK
                : CONTENT_SETTING_ALLOW);

  // This is blocked because both patterns match.
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting("*", kURL, CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_EQ(
      settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(), GURL(kURL),
                                net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED, 1);
}

TEST_F(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTestP,
       GetCookieSettingOverridePreservesSessionOnlySetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_SESSION_ONLY));
}

TEST_F(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  EXPECT_EQ(settings.GetCookieSetting(GURL(kURL), net::SiteForCookies(),
                                      GURL(kOtherURL),
                                      net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
}

// The Storage Access API should unblock storage access that would otherwise be
// blocked.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAUnblocks) {
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
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
      1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT,
      IsStorageAccessGrantEligibleViaAPI() ||
              IsStorageAccessGrantEligibleViaHeader()
          ? 1
          : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
      IsStorageAccessGrantEligibleViaAPI() ||
              IsStorageAccessGrantEligibleViaHeader()
          ? 1
          : 2);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      settings.GetCookieSetting(third_url, net::SiteForCookies(), top_level_url,
                                GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);

  // If third-party cookies are blocked, SAA grant takes precedence over
  // possible override to allow 3PCs.
  {
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(
        settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                  GetCookieSettingOverrides(), nullptr),
        SettingWithSaaOverride(CONTENT_SETTING_ALLOW));
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaOverride(),
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
    EXPECT_EQ(
        settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                  GetCookieSettingOverrides(), nullptr),
        CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
  }
}

TEST_P(CookieSettingsTestP, GetCookieSettingSAAUnblocksViaFedCM) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  GURL third_url = GURL(kDomainURL);

  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);

  settings.set_content_settings(
      ContentSettingsType::FEDERATED_IDENTITY_SHARING,
      {CreateSetting(net::SchemefulSite(url).Serialize(),
                     net::SchemefulSite(top_level_url).Serialize(),
                     CONTENT_SETTING_ALLOW)});

  // When requesting our setting for the embedder/top-level combination our
  // grant is for access should be allowed. For any other domain pairs access
  // should still be blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaViaAPI(CONTENT_SETTING_ALLOW));
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaViaAPI(), 1);

  // Grants are not bidirectional.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT,
      IsStorageAccessGrantEligibleViaAPI() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram, BlockedStorageAccessResultWithSaaViaAPI(),
      IsStorageAccessGrantEligibleViaAPI() ? 1 : 2);

  // Unrelated contexts do not get access.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      settings.GetCookieSetting(third_url, net::SiteForCookies(), top_level_url,
                                GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);

  // If cookies are globally blocked, SAA grants and 3PC override
  // should both be ignored.
  {
    settings.set_content_settings(
        ContentSettingsType::COOKIES,
        {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});
    settings.set_block_third_party_cookies(true);
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(
        settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                  GetCookieSettingOverrides(), nullptr),
        CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
  }
}

// The Top-Level Storage Access API should unblock storage access that would
// otherwise be blocked.
TEST_P(CookieSettingsTestP, GetCookieSettingTopLevelStorageAccessUnblocks) {
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
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevelSaaOverride());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevelSaaOverride(), 1);

  // Check the cookie setting that does not match the top-level storage access
  // grant--the |top_level_url| granting access to |url| is now being loaded
  // under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  // TODO(crbug.com/40246640): Separate metrics between StorageAccessAPI
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
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      settings.GetCookieSetting(third_url, net::SiteForCookies(), top_level_url,
                                GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);

  // If third-party cookies are blocked, Top-Level Storage Access grant takes
  // precedence over possible override to allow third-party cookies.
  {
    base::HistogramTester histogram_tester_2;
    EXPECT_EQ(
        settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
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
    EXPECT_EQ(
        settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                  GetCookieSettingOverrides(), nullptr),
        CONTENT_SETTING_BLOCK);
    histogram_tester_2.ExpectUniqueSample(
        kAllowedRequestsHistogram,
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
  }
}

// Subdomains of the granted embedding url should not gain access if a valid
// grant exists.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAResourceWildcards) {
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

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(settings.GetCookieSetting(GURL(kSubDomainURL),
                                      net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists.
TEST_P(CookieSettingsTestP, GetCookieSettingSAATopLevelWildcards) {
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

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(
      settings.GetCookieSetting(url, net::SiteForCookies(), GURL(kSubDomainURL),
                                GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
}

// Any Storage Access API grant should not override an explicit setting to block
// cookie access.
TEST_P(CookieSettingsTestP, GetCookieSettingSAARespectsSettings) {
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

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_P(CookieSettingsTestP, GetCookieSettingSAAExpiredGrant) {
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
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithSaaOverride(CONTENT_SETTING_ALLOW));

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(
      "foo.com", net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateExceptionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("https://foo.com", "*", CONTENT_SETTING_SESSION_ONLY)});

  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(
      "foo.com", net::CookieSourceScheme::kSecure));
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(
      "other.com", net::CookieSourceScheme::kSecure));
}

TEST_F(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(
      "foo.com", net::CookieSourceScheme::kNonSecure));
}

TEST_F(CookieSettingsTest, GetCookieSettingSecureOriginCookiesAllowed) {
  CookieSettings settings;
  settings.set_secure_origin_cookies_allowed_schemes({"chrome"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("https://foo.com") /* url */, net::SiteForCookies(),
                GURL("chrome://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome://foo") /* url */, net::SiteForCookies(),
                GURL("https://foo.com") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */, net::SiteForCookies(),
                GURL("chrome://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingWithThirdPartyCookiesAllowedScheme) {
  CookieSettings settings;
  settings.set_third_party_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */, net::SiteForCookies(),
                GURL("chrome-extension://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */, net::SiteForCookies(),
                GURL("other-scheme://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://foo") /* url */, net::SiteForCookies(),
                GURL("http://foo.com") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, GetCookieSettingMatchingSchemeCookiesAllowed) {
  CookieSettings settings;
  settings.set_matching_scheme_cookies_allowed_schemes({"chrome-extension"});
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://bar") /* url */, net::SiteForCookies(),
                GURL("chrome-extension://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("http://foo.com") /* url */, net::SiteForCookies(),
                GURL("chrome-extension://foo") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(settings.GetCookieSetting(
                GURL("chrome-extension://foo") /* url */, net::SiteForCookies(),
                GURL("http://foo.com") /* first_party_url */,
                net::CookieSettingOverrides()),
            CONTENT_SETTING_BLOCK);
}

TEST_F(CookieSettingsTest, LegacyCookieAccessDefault) {
  CookieSettings settings;

  EXPECT_EQ(settings.GetSettingForLegacyCookieAccess(kDomain),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            settings.GetCookieAccessSemanticsForDomain(kDomain));
}

TEST_F(CookieSettingsTest, CookieAccessSemanticsForDomain) {
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

TEST_F(CookieSettingsTest, CookieAccessSemanticsForDomainWithWildcard) {
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

TEST_P(CookieSettingsTestP, IsPrivacyModeEnabled) {
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
          IsStorageAccessGrantEligibleViaAPI() ||
                  IsStorageAccessGrantEligibleViaHeader()
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

TEST_P(CookieSettingsTestP, IsCookieAccessible_SameSiteNoneCookie) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kURL);

  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
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
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_FALSE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));
  EXPECT_TRUE(status.HasExclusionReason(
      IsTPCDEnabled() ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
                      : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));

  // Note that the SiteForCookies matches nothing, so this is a third-party
  // context even though the `url` matches the `top_frame_origin`.
  status.ResetForTesting();
  EXPECT_EQ(settings.IsCookieAccessible(
                *cookie, GURL(kURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            IsStorageAccessGrantEligibleViaAPI() ||
                IsStorageAccessGrantEligibleViaHeader());
  EXPECT_THAT(status.HasExclusionReason(
                  IsTPCDEnabled()
                      ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
                      : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES),
              Not(IsStorageAccessGrantEligibleViaAPI() ||
                  IsStorageAccessGrantEligibleViaHeader()));
  EXPECT_THAT(status.exemption_reason() ==
                  net::CookieInclusionStatus::ExemptionReason::kStorageAccess,
              IsStorageAccessGrantEligibleViaAPI() ||
                  IsStorageAccessGrantEligibleViaHeader());

  // No override can overrule a site-specific setting.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  status.ResetForTesting();
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
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
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.HasOnlyExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));
}

TEST_P(CookieSettingsTestP, IsCookieAccessible_SameSiteLaxCookie) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalCookie("name", kURL);

  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_FALSE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));

  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  status.ResetForTesting();
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  if (IsTPCDEnabled()) {
    // This cookie is blocked due to SameSite=Lax, thus the following reasons
    // are irrelevant.
    EXPECT_FALSE(status.HasWarningReason(
        net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));
    EXPECT_FALSE(status.HasExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT));
    EXPECT_FALSE(status.HasExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));
  } else {
    EXPECT_TRUE(status.HasExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));
  }
  // Note that the SiteForCookies matches nothing, so this is a third-party
  // context even though the `url` matches the `top_frame_origin`.
  status.ResetForTesting();
  EXPECT_EQ(settings.IsCookieAccessible(
                *cookie, GURL(kURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            IsStorageAccessGrantEligibleViaAPI() ||
                IsStorageAccessGrantEligibleViaHeader());
  // This cookie is not SameSite=None cookie, thus exemption reason is
  // irrelevant.
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);

  // No override can overrule a site-specific setting.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  status.ResetForTesting();
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
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
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.HasExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES));
}

TEST_P(CookieSettingsTestP, IsCookieAccessible_PartitionedCookies) {
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
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // If third-party cookie blocking is disabled, the partitioned cookie should
  // still be available
  settings.set_block_third_party_cookies(false);
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // If there is a site-specific content setting blocking cookies, then
  // partitioned cookies should not be available.
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kURL, "*", CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // If third-party cookie blocking is enabled and there is a site-specific
  // content setting blocking the top-frame origin's cookies, then the
  // partitioned cookie should be blocked as well.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_FALSE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // If third-party cookie blocking is enabled and there is a site-specific
  // setting for the top-frame origin that only applies on an unrelated site,
  // then the partitioned cookie should still be allowed.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kUnrelatedURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // If third-party cookie blocking is enabled and there is a matching Storage
  // Access setting whose value is BLOCK, then the partitioned cookie should
  // still be allowed.
  settings.set_block_third_party_cookies(true);
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));

  // Partitioned cookies are not affected by 3pc phaseout, so the
  // *_THIRD_PARTY_PHASEOUT warning/exclusion reason is irrelevant.
  EXPECT_FALSE(status.HasWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT));
  EXPECT_FALSE(status.HasExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT));
}

TEST_P(CookieSettingsTestP, IsCookieAccessible_NoneExemptionReason) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  std::unique_ptr<net::CanonicalCookie> partitioned_cookie =
      MakeCanonicalSameSiteNoneCookie(
          "__Host-partitioned", kURL,
          net::CookiePartitionKey::FromURLForTesting(GURL(kOtherURL)));
  std::unique_ptr<net::CanonicalCookie> samesitelax_cookie =
      MakeCanonicalCookie("samesite_lax", kURL);

  // Precautionary - partitioned cookies should be allowed with no exemption
  // reason.
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.exemption_reason() ==
              net::CookieInclusionStatus::ExemptionReason::kNone);

  // Sets exemptions
  if (IsStorageAccessGrantEligibleViaAPI() ||
      IsStorageAccessGrantEligibleViaHeader()) {
    // Sets the storage access granted by Storage Access API.
    settings.set_content_settings(
        ContentSettingsType::STORAGE_ACCESS,
        {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  } else if (IsTopLevelStorageAccessGrantEligible()) {
    // Sets the storage access granted by Top-Level Storage Access API.
    settings.set_content_settings(
        ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
        {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  } else {
    // Sets a site-specific setting
    settings.set_content_settings(
        ContentSettingsType::COOKIES,
        {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  }
  // Partitioned cookies should be allowed with no exemption reason even with
  // the exemptions.
  status.ResetForTesting();
  EXPECT_TRUE(settings.IsCookieAccessible(
      *partitioned_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.exemption_reason() ==
              net::CookieInclusionStatus::ExemptionReason::kNone);

  // non-SameSite=None cookies should have no exemption reason.
  status.ResetForTesting();
  EXPECT_TRUE(settings.IsCookieAccessible(
      *samesitelax_cookie, GURL(kURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kOtherURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.exemption_reason() ==
              net::CookieInclusionStatus::ExemptionReason::kNone);
}

TEST_P(CookieSettingsTestP, IsCookieAccessible_SitesInFirstPartySets) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  url::Origin top_level_origin = url::Origin::Create(GURL(kRwsOwnerURL));

  net::SchemefulSite primary((GURL(kRwsOwnerURL)));
  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          std::nullopt);

  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kRwsMemberURL);

  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kRwsMemberURL), net::SiteForCookies(), top_level_origin,
      net::FirstPartySetMetadata(), GetCookieSettingOverrides(), &status));
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {IsTPCDEnabled()
           ? net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT
           : net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));

  status.ResetForTesting();
  // EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET should be added with the
  // FirstPartySetMetadata indicating the cookie url and the top-level url are
  // in the same RWS.
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, GURL(kRwsMemberURL), net::SiteForCookies(), top_level_origin,
      net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
      GetCookieSettingOverrides(), &status));
  if (IsTPCDEnabled()) {
    EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
        {net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT,
         net::CookieInclusionStatus::
             EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET}));
  } else {
    EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
        {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}));
  }
}

TEST_P(CookieSettingsTestP, AnnotateAndMoveUserBlockedCookies_CrossSiteEmbed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kURL), {}},
      {*MakeCanonicalCookie("cookie", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))},
      {*MakeCanonicalCookie("excluded_samesitelax", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))},
  };
  url::Origin origin = url::Origin::Create(GURL(kOtherURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(
                    /*frame_entry=*/nullptr,
                    /*top_frame_entry=*/nullptr),
                GetCookieSettingOverrides(), maybe_included_cookies,
                excluded_cookies),
            IsTPCDEnabled());

  if (IsTPCDEnabled()) {
    EXPECT_THAT(
        maybe_included_cookies,
        UnorderedElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("cookie"),
            // This cookie really ought to be excluded based on SameSite rules,
            // but since those are not enforced by CookieSettings, and
            // CookieSettings doesn't add 3PCD-related exclusion reasons to
            // non-SameSite=None cookies, we don't expect an exclusion reason
            // for this cookie.
            MatchesCookieAccessResult(
                AllOf(net::IsInclude(),
                      Not(net::HasWarningReason(
                          net::CookieInclusionStatus::WarningReason::
                              WARN_THIRD_PARTY_PHASEOUT)),
                      net::HasExactlyExemptionReason(
                          net::CookieInclusionStatus::ExemptionReason::kNone)),
                _, _, _))));

    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    net::HasExclusionReason(net::CookieInclusionStatus::
                                                EXCLUDE_THIRD_PARTY_PHASEOUT),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_samesitelax"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY}),
                    _, _, _))));
  } else {
    // The 3pcs are blocked by user.
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
                            net::CookieInclusionStatus::
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
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_samesitelax"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY,
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES,
                        }),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_F(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_CrossSiteEmbed_3PCAllowed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(false);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalCookie("lax_by_default", kURL), {}},
      {*MakeCanonicalSameSiteNoneCookie("third_party", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};
  url::Origin origin = url::Origin::Create(GURL(kOtherURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  // Verify that the allowed cookie has the expected warning reason.
  EXPECT_THAT(
      maybe_included_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("lax_by_default"),
              MatchesCookieAccessResult(
                  AllOf(net::IsInclude(),
                        Not(net::HasWarningReason(
                            net::CookieInclusionStatus::WarningReason::
                                WARN_THIRD_PARTY_PHASEOUT))),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("third_party"),
              MatchesCookieAccessResult(
                  AllOf(net::IsInclude(),
                        net::HasExactlyWarningReasonsForTesting(
                            std::vector<
                                net::CookieInclusionStatus::WarningReason>{
                                net::CookieInclusionStatus::WarningReason::
                                    WARN_THIRD_PARTY_PHASEOUT})),
                  _, _, _))));
}

TEST_F(CookieSettingsTest,
       AnnotateAndMoveUserBlockedCookies_SameSiteEmbed_3PCAllowed) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(false);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("cookie", kDomainURL), {}}};
  net::CookieAccessResultList excluded_cookies = {};
  url::Origin origin = url::Origin::Create(GURL(kDomainURL));

  // This is a first-party context.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kSubDomainURL), net::SiteForCookies(net::SchemefulSite(origin)),
      &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  // Verify that the allowed cookie does not have the 3PC warning reason.
  EXPECT_THAT(
      maybe_included_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("cookie"),
          MatchesCookieAccessResult(
              AllOf(net::IsInclude(),
                    Not(net::HasWarningReason(
                        net::CookieInclusionStatus::WarningReason::
                            WARN_THIRD_PARTY_PHASEOUT)),
                    net::HasExactlyExemptionReason(
                        net::CookieInclusionStatus::ExemptionReason::kNone)),
              _, _, _))));

  // This is a third-party context, even though the request URL and the
  // top-frame URL are same-site with each other.
  maybe_included_cookies[0].access_result.status = net::CookieInclusionStatus();
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kSubDomainURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

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

TEST_P(CookieSettingsTestP,
       AnnotateAndMoveUserBlockedCookies_SameSiteEmbed_FirstPartyContext) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kURL), {}},
      {*MakeCanonicalCookie("cookie", kURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))},
      {*MakeCanonicalCookie("excluded_samesitelax", kURL),
       // The ExclusionReason below is irrelevant, as long as there is
       // one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))},
  };
  url::Origin origin = url::Origin::Create(GURL(kURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_TRUE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kURL), net::SiteForCookies::FromUrl(GURL(kURL)), &origin,
      net::FirstPartySetMetadata(
          /*frame_entry=*/nullptr,
          /*top_frame_entry=*/nullptr),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_THAT(
      maybe_included_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("cookie"),
              // This cookie really ought to be excluded based on SameSite
              // rules, but those are not enforced by CookieSettings.
              MatchesCookieAccessResult(
                  AllOf(
                      net::IsInclude(),
                      Not(net::HasWarningReason(
                          net::CookieInclusionStatus::WarningReason::
                              WARN_THIRD_PARTY_PHASEOUT)),
                      net::HasExactlyExemptionReason(
                          net::CookieInclusionStatus::ExemptionReason::kNone)),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("third_party"),
              MatchesCookieAccessResult(
                  AllOf(
                      net::IsInclude(),
                      Not(net::HasWarningReason(
                          net::CookieInclusionStatus::WarningReason::
                              WARN_THIRD_PARTY_PHASEOUT)),
                      net::HasExactlyExemptionReason(
                          net::CookieInclusionStatus::ExemptionReason::kNone)),
                  _, _, _))));

  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_other"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              net::MatchesCookieWithName("excluded_samesitelax"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY}),
                  _, _, _))));
}

TEST_P(CookieSettingsTestP,
       AnnotateAndMoveUserBlockedCookies_SameSiteEmbed_ThirdPartyContext) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("cookie", kDomainURL), {}},
      {*MakeCanonicalCookie("samesite_lax", kDomainURL), {}},
  };
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kDomainURL),
       // The ExclusionReason below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};
  url::Origin origin = url::Origin::Create(GURL(kDomainURL));

  const bool expected_any_allowed = IsStorageAccessGrantEligibleViaAPI() ||
                                    IsStorageAccessGrantEligibleViaHeader() ||
                                    IsTPCDEnabled();

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

  if (IsStorageAccessGrantEligibleViaAPI() ||
      IsStorageAccessGrantEligibleViaHeader()) {
    EXPECT_THAT(
        maybe_included_cookies,
        ElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("cookie"),
                MatchesCookieAccessResult(
                    AllOf(net::IsInclude(),
                          Not(net::HasWarningReason(
                              net::CookieInclusionStatus::WarningReason::
                                  WARN_THIRD_PARTY_PHASEOUT)),
                          net::HasExactlyExemptionReason(
                              net::CookieInclusionStatus::ExemptionReason::
                                  kStorageAccess)),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("samesite_lax"),
                MatchesCookieAccessResult(
                    AllOf(net::IsInclude(),
                          Not(net::HasWarningReason(
                              net::CookieInclusionStatus::WarningReason::
                                  WARN_THIRD_PARTY_PHASEOUT)),
                          net::HasExactlyExemptionReason(
                              net::CookieInclusionStatus::ExemptionReason::
                                  kNone)),
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
  } else if (IsTPCDEnabled()) {
    EXPECT_THAT(
        maybe_included_cookies,
        UnorderedElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("samesite_lax"),
            // This cookie really ought to be excluded based on SameSite
            // rules, but since those are not enforced by CookieSettings, and
            // CookieSettings doesn't add 3PCD-related exclusion reasons to
            // non-SameSite=None cookies, we don't expect an exclusion reason
            // for this cookie.
            MatchesCookieAccessResult(
                AllOf(
                    net::IsInclude(),
                    Not(net::HasWarningReason(
                        net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)),
                    net::HasExactlyExemptionReason(
                        net::CookieInclusionStatus::ExemptionReason::kNone)),
                _, _, _))));
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("cookie"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_THIRD_PARTY_PHASEOUT}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY}),
                    _, _, _))));
  } else {
    // 3pc is blocked by user.
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
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("samesite_lax"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
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
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTestP,
       AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  if (IsTrackingProtectionEnabledFor3pcd()) {
    settings.set_tracking_protection_enabled_for_3pcd(true);
  }

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party_but_member",
                                        kRwsMemberURL),
       {}}};
  net::CookieAccessResultList excluded_cookies = {};

  url::Origin origin = url::Origin::Create(GURL(kRwsOwnerURL));
  net::SchemefulSite primary((GURL(kRwsOwnerURL)));

  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          std::nullopt);

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kRwsMemberURL), net::SiteForCookies(), &origin,
      net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_EQ(0u, maybe_included_cookies.size());
  EXPECT_THAT(
      excluded_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("third_party_but_member"),
          MatchesCookieAccessResult(
              HasExactlyExclusionReasonsForTesting(
                  IsForceThirdPartyCookieBlockingFlagEnabled() ||
                          IsTrackingProtectionEnabledFor3pcd()
                      ? std::vector<
                            net::CookieInclusionStatus::
                                ExclusionReason>{
                                    net::CookieInclusionStatus::
                                        EXCLUDE_THIRD_PARTY_PHASEOUT,
                                    net::CookieInclusionStatus::
                                        EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                                }
                      : std::vector<
                            net::CookieInclusionStatus::
                                ExclusionReason>{net::CookieInclusionStatus::
                                                     EXCLUDE_USER_PREFERENCES}),
              _, _, _))));
}

TEST_P(
    CookieSettingsTestP,
    AnnotateAndMoveUserBlockedCookies_SitesInFirstPartySet_FirstPartyURLBlocked) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting(kRwsOwnerURL, kRwsOwnerURL, CONTENT_SETTING_BLOCK)});

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("third_party_but_member", kRwsMemberURL);

  url::Origin top_frame_origin = url::Origin::Create(GURL(kRwsOwnerURL));

  net::SchemefulSite primary((GURL(kRwsOwnerURL)));
  net::FirstPartySetEntry frame_entry(primary, net::SiteType::kAssociated, 1u);
  net::FirstPartySetEntry top_frame_entry(primary, net::SiteType::kPrimary,
                                          std::nullopt);
  // Without third-party-cookie-blocking enabled, the cookie is accessible, even
  // though cookies are blocked for the top-level URL.
  ASSERT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kRwsMemberURL), net::SiteForCookies(), top_frame_origin,
      net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
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

  EXPECT_FALSE(settings.AnnotateAndMoveUserBlockedCookies(
      GURL(kRwsMemberURL), net::SiteForCookies(), &top_frame_origin,
      net::FirstPartySetMetadata(&frame_entry, &top_frame_entry),
      GetCookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

  EXPECT_EQ(0u, maybe_included_cookies.size());

  EXPECT_THAT(
      excluded_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
          net::MatchesCookieWithName("third_party_but_member"),
          MatchesCookieAccessResult(
              HasExactlyExclusionReasonsForTesting(
                  IsForceThirdPartyCookieBlockingFlagEnabled() ||
                          IsTrackingProtectionEnabledFor3pcd()
                      ? std::vector<
                            net::CookieInclusionStatus::
                                ExclusionReason>{
                                    net::CookieInclusionStatus::
                                                EXCLUDE_THIRD_PARTY_PHASEOUT,
                                    net::CookieInclusionStatus::
                                                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
                                }
                      : std::vector<
                            net::CookieInclusionStatus::
                                ExclusionReason>{net::CookieInclusionStatus::
                                                     EXCLUDE_USER_PREFERENCES}),
              _, _, _))));
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

TEST_F(CookieSettingsTest,
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
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
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
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
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
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));

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
      net::CookieSettingOverrides(), maybe_included_cookies, excluded_cookies));
  EXPECT_THAT(maybe_included_cookies,
              ElementsAre(MatchesCookieWithAccessResult(
                  net::MatchesCookieWithName("__Host-partitioned"),
                  MatchesCookieAccessResult(net::IsInclude(), _, _, _))));
  EXPECT_THAT(excluded_cookies, IsEmpty());
}

TEST_F(CookieSettingsTest, GetStorageAccessStatus) {
  CookieSettings settings;
  GURL url = GURL(kURL);
  url::Origin top_frame_origin = url::Origin::Create(GURL(kOtherURL));
  settings.set_block_third_party_cookies(true);

  EXPECT_EQ(settings.GetStorageAccessStatus(
                url, net::SiteForCookies::FromUrl(url),
                url::Origin::Create(url), net::CookieSettingOverrides()),
            std::nullopt);

  EXPECT_EQ(
      settings.GetStorageAccessStatus(
          url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
          net::CookieSettingOverrides(
              {net::CookieSettingOverride::kStorageAccessGrantEligible})),
      std::nullopt);

  EXPECT_EQ(settings.GetStorageAccessStatus(url, net::SiteForCookies(),
                                            top_frame_origin,
                                            net::CookieSettingOverrides()),
            net::cookie_util::StorageAccessStatus::kNone);

  EXPECT_EQ(settings.GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::kStorageAccessGrantEligible})),
            net::cookie_util::StorageAccessStatus::kNone);

  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetStorageAccessStatus(url, net::SiteForCookies(),
                                            top_frame_origin,
                                            net::CookieSettingOverrides()),
            net::cookie_util::StorageAccessStatus::kInactive);

  EXPECT_EQ(settings.GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::kStorageAccessGrantEligible})),
            net::cookie_util::StorageAccessStatus::kActive);

  EXPECT_EQ(settings.GetStorageAccessStatus(
                url, net::SiteForCookies(), top_frame_origin,
                net::CookieSettingOverrides(
                    {net::CookieSettingOverride::
                         kStorageAccessGrantEligibleViaHeader})),
            net::cookie_util::StorageAccessStatus::kActive);
}

TEST_F(CookieSettingsTest,
       StorageAccessHeaderOriginTrialSettingDefaultBlocked) {
  CookieSettings settings;
  EXPECT_FALSE(settings.IsStorageAccessHeadersEnabled(
      GURL(kURL), url::Origin::Create(GURL(kOtherURL))));
}

TEST_F(CookieSettingsTest,
       StorageAccessHeaderOriginTrialSettingAllowedWhenSet) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});

  EXPECT_TRUE(settings.IsStorageAccessHeadersEnabled(
      GURL(kURL), url::Origin::Create(GURL(kOtherURL))));
}

TEST_F(CookieSettingsTest,
       StorageAccessHeaderOriginTrialSettingUnaffectedByIrrelevantSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});

  EXPECT_FALSE(settings.IsStorageAccessHeadersEnabled(
      GURL(kURL), url::Origin::Create(GURL(kOtherURL))));
}

TEST_F(
    CookieSettingsTest,
    StorageAccessHeaderOriginTrialSettingUnaffectedBySettingForDifferentPair) {
  CookieSettings settings;
  settings.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
      {CreateSetting(kUnrelatedURL, kOtherURL, CONTENT_SETTING_ALLOW)});

  EXPECT_FALSE(settings.IsStorageAccessHeadersEnabled(
      GURL(kURL), url::Origin::Create(GURL(kOtherURL))));
}

// NOTE: These tests will fail if their FINAL name is of length greater than 256
// characters. Thus, try to avoid (unnecessary) generalized parameterization
// when possible.
std::string CustomTestName(
    const testing::TestParamInfo<CookieSettingsTestP::ParamType>& info) {
  std::stringstream custom_test_name;
  // clang-format off
  custom_test_name
      << "GrantSource_"
      << std::get<TestVariables::kGrantSource>(info.param)
      << "_BlockSource_"
      << std::get<TestVariables::kBlockSource>(info.param);
  // clang-format on
  return custom_test_name.str();
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTestP,
    testing::Combine(testing::Range(GrantSource::kNoneGranted,
                                    GrantSource::kGrantSourceCount),
                     testing::Range(BlockSource::kNoneBlocked,
                                    BlockSource::kBlockSourceCount)),
    CustomTestName);

class CookieSettingsTpcdMetadataGrantsTest
    : public CookieSettingsTestBase,
      public testing::WithParamInterface</* net::features::kTpcdMetadataGrants:
                                          */
                                         bool> {
 public:
  CookieSettingsTpcdMetadataGrantsTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantEligible()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsTpcdMetadataGrantEligible() const { return GetParam(); }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    return overrides;
  }

  ContentSetting SettingWith3pcdMetadataGrantEligibleOverride() const {
    return IsTpcdMetadataGrantEligible() ? CONTENT_SETTING_ALLOW
                                         : CONTENT_SETTING_BLOCK;
  }

  // The storage access result would be blocked if not for a
  // `net::features::kTpcdMetadataGrants` enablement.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWith3pcdMetadataGrantOverride() const {
    if (IsTpcdMetadataGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_3PCD_METADATA_GRANT;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }
};

TEST_P(CookieSettingsTpcdMetadataGrantsTest, Grants) {
  GURL first_party_url = GURL(kURL);
  GURL third_party_url_1 = GURL(kOtherURL);
  GURL third_party_url_2 = GURL(kDomainURL);

  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Allowlisting.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants(
      {CreateSetting(third_party_url_1.host(), first_party_url.host(),
                     CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(settings.GetCookieSetting(third_party_url_1, net::SiteForCookies(),
                                      first_party_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWith3pcdMetadataGrantEligibleOverride());

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdMetadataGrantOverride(), 1);

  EXPECT_EQ(settings.GetCookieSetting(first_party_url, net::SiteForCookies(),
                                      third_party_url_1,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD_METADATA_GRANT,
      IsTpcdMetadataGrantEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdMetadataGrantOverride(),
      IsTpcdMetadataGrantEligible() ? 1 : 2);

  EXPECT_EQ(settings.GetCookieSetting(third_party_url_2, net::SiteForCookies(),
                                      first_party_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTpcdMetadataGrantsTest, IsCookieAccessible) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Allowlisting.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants({CreateSetting(kOtherURL, kURL, CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);

  EXPECT_EQ(settings.IsCookieAccessible(
                *cookie, GURL(kOtherURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            IsTpcdMetadataGrantEligible());
  EXPECT_EQ(status.exemption_reason() ==
                net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata,
            IsTpcdMetadataGrantEligible());
}

TEST_P(CookieSettingsTpcdMetadataGrantsTest,
       AnnotateAndMoveUserBlockedCookies) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Allowlisting.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants({CreateSetting(kOtherURL, kURL, CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kOtherURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kOtherURL),
       // The ExclusionReason below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};

  url::Origin origin = url::Origin::Create(GURL(kURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kOtherURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(), GetCookieSettingOverrides(),
                maybe_included_cookies, excluded_cookies),
            IsTpcdMetadataGrantEligible());

  if (IsTpcdMetadataGrantEligible()) {
    EXPECT_THAT(maybe_included_cookies,
                ElementsAre(MatchesCookieWithAccessResult(
                    net::MatchesCookieWithName("third_party"),
                    MatchesCookieAccessResult(
                        AllOf(net::IsInclude(),
                              net::HasExactlyExemptionReason(
                                  net::CookieInclusionStatus::ExemptionReason::
                                      k3PCDMetadata)),
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
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY,
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTpcdMetadataGrantsTest, ExplicitSettingPreserved) {
  GURL first_party_url = GURL(kURL);
  GURL third_party_url = GURL(kOtherURL);
  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Explicit setting.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", first_party_url.host(), CONTENT_SETTING_BLOCK)});

  // Allowlisting.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants({CreateSetting(
      third_party_url.host(), first_party_url.host(), CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(settings.GetCookieSetting(third_party_url, net::SiteForCookies(),
                                      first_party_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         CookieSettingsTpcdMetadataGrantsTest,
                         testing::Bool());

class CookieSettingsTpcdTrialTest
    : public CookieSettingsTestBase,
      public testing::WithParamInterface</* net::features::kTpcdTrialSettings:
                                          */
                                         bool> {
 public:
  CookieSettingsTpcdTrialTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (Is3pcdTrialEligible()) {
      enabled_features.push_back(net::features::kTpcdTrialSettings);
    } else {
      disabled_features.push_back(net::features::kTpcdTrialSettings);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool Is3pcdTrialEligible() const { return GetParam(); }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    return overrides;
  }

  // The cookie access would be blocked if not for a
  // `ContentSettingsType::TPCD_TRIAL` setting.
  ContentSetting SettingWith3pcdTrialSetting() const {
    return Is3pcdTrialEligible() ? CONTENT_SETTING_ALLOW
                                 : CONTENT_SETTING_BLOCK;
  }

  // The storage access result would be blocked if not for a
  // `ContentSettingsType::TPCD_TRIAL` setting.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWith3pcdTrialSetting() const {
    if (Is3pcdTrialEligible()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD_TRIAL;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }
};

TEST_P(CookieSettingsTpcdTrialTest, OverrideDefaultBlock3pcSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  GURL third_url = GURL(kDomainURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TPCD_TRIAL,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWith3pcdTrialSetting());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdTrialSetting(), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD_TRIAL,
      Is3pcdTrialEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWith3pcdTrialSetting(),
      Is3pcdTrialEligible() ? 1 : 2);

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), third_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      settings.GetCookieSetting(third_url, net::SiteForCookies(), top_level_url,
                                GetCookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);

  // Check cookie setting override to skip 3PCD trial settings.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTPCDTrial);
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      overrides, nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTpcdTrialTest, IsCookieAccessible) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TPCD_TRIAL,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);

  EXPECT_EQ(settings.IsCookieAccessible(
                *cookie, GURL(kOtherURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            Is3pcdTrialEligible());
  EXPECT_EQ(
      status.exemption_reason() ==
          net::CookieInclusionStatus::ExemptionReason::k3PCDDeprecationTrial,
      Is3pcdTrialEligible());
}

TEST_P(CookieSettingsTpcdTrialTest, AnnotateAndMoveUserBlockedCookies) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TPCD_TRIAL,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kOtherURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kOtherURL),
       // The ExclusionReason below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};

  url::Origin origin = url::Origin::Create(GURL(kURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kOtherURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(), GetCookieSettingOverrides(),
                maybe_included_cookies, excluded_cookies),
            Is3pcdTrialEligible());

  if (Is3pcdTrialEligible()) {
    EXPECT_THAT(maybe_included_cookies,
                ElementsAre(MatchesCookieWithAccessResult(
                    net::MatchesCookieWithName("third_party"),
                    MatchesCookieAccessResult(
                        AllOf(net::IsInclude(),
                              net::HasExactlyExemptionReason(
                                  net::CookieInclusionStatus::ExemptionReason::
                                      k3PCDDeprecationTrial)),
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
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY,
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTpcdTrialTest, PreserveBlockAllCookiesSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Set default cookie setting to block ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  settings.set_content_settings(
      ContentSettingsType::TPCD_TRIAL,
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});

  base::HistogramTester histogram_tester;

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

TEST_P(CookieSettingsTpcdTrialTest, PreserveExplicitBlock3pcSetting) {
  GURL first_party_url = GURL(kURL);
  GURL third_party_url = GURL(kOtherURL);
  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Explicit setting.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", first_party_url.host(), CONTENT_SETTING_BLOCK)});

  // Allowlisting.
  settings.set_content_settings(
      ContentSettingsType::TPCD_TRIAL,
      {CreateSetting(third_party_url.host(), first_party_url.host(),
                     CONTENT_SETTING_ALLOW)});

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(settings.GetCookieSetting(third_party_url, net::SiteForCookies(),
                                      first_party_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         CookieSettingsTpcdTrialTest,
                         testing::Bool());

class CookieSettingsTopLevelTpcdTrialTest
    : public CookieSettingsTestBase,
      public testing::
          WithParamInterface</* net::features::kTopLevelTpcdTrialSettings:
                              */
                             bool> {
 public:
  CookieSettingsTopLevelTpcdTrialTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTopLevel3pcdTrialEligible()) {
      enabled_features.push_back(net::features::kTopLevelTpcdTrialSettings);
    } else {
      disabled_features.push_back(net::features::kTopLevelTpcdTrialSettings);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsTopLevel3pcdTrialEligible() const { return GetParam(); }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    return overrides;
  }

  // The cookie access would be blocked if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_TRIAL` setting.
  ContentSetting SettingWithTopLevel3pcdTrialSetting() const {
    return IsTopLevel3pcdTrialEligible() ? CONTENT_SETTING_ALLOW
                                         : CONTENT_SETTING_BLOCK;
  }

  // The storage access result would be blocked if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_TRIAL` setting.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevel3pcdTrialSetting() const {
    if (IsTopLevel3pcdTrialEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_3PCD_TRIAL;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // The default scope for |ContentSettingsType::TOP_LEVEL_TPCD_TRIAL| is
  // |WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE|, so this returns a setting of
  // that form.
  ContentSettingPatternSource CreateSettingForTopLevelTpcdTrial(
      GURL top_level_url,
      ContentSetting setting) {
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromURLNoWildcard(top_level_url),
        ContentSettingsPattern::Wildcard(), base::Value(setting),
        content_settings::ProviderType::kNone, false /* incognito */);
  }
};

TEST_P(CookieSettingsTopLevelTpcdTrialTest, OverrideDefaultBlock3pcSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  CookieSettings settings;
  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                                {CreateSettingForTopLevelTpcdTrial(
                                    top_level_url, CONTENT_SETTING_ALLOW)});

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdTrialSetting());
  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevel3pcdTrialSetting(), 1);

  // Invalid pair where the |top_level_url| granting access to embedded
  // resources is now being loaded under |url| as the top level url.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectBucketCount(kAllowedRequestsHistogram,
                                     net::cookie_util::StorageAccessResult::
                                         ACCESS_ALLOWED_TOP_LEVEL_3PCD_TRIAL,
                                     IsTopLevel3pcdTrialEligible() ? 1 : 0);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      BlockedStorageAccessResultWithTopLevel3pcdTrialSetting(),
      IsTopLevel3pcdTrialEligible() ? 1 : 2);

  // Invalid pairs where a |url| is the top-level site.
  EXPECT_EQ(settings.GetCookieSetting(top_level_url, net::SiteForCookies(), url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Check cookie setting override to skip top-level 3PCD trial settings.
  auto overrides = GetCookieSettingOverrides();
  overrides.Put(net::CookieSettingOverride::kSkipTopLevelTPCDTrial);
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      overrides, nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTopLevelTpcdTrialTest, IsCookieAccessible) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
      {CreateSettingForTopLevelTpcdTrial(GURL(kURL), CONTENT_SETTING_ALLOW)});

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);

  EXPECT_EQ(settings.IsCookieAccessible(
                *cookie, GURL(kOtherURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            IsTopLevel3pcdTrialEligible());
  EXPECT_EQ(status.exemption_reason() ==
                net::CookieInclusionStatus::ExemptionReason::
                    kTopLevel3PCDDeprecationTrial,
            IsTopLevel3pcdTrialEligible());
}

TEST_P(CookieSettingsTopLevelTpcdTrialTest, AnnotateAndMoveUserBlockedCookies) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  settings.set_content_settings(
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
      {CreateSettingForTopLevelTpcdTrial(GURL(kURL), CONTENT_SETTING_ALLOW)});

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kOtherURL), {}}};
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kOtherURL),
       // The ExclusionReason below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};

  url::Origin origin = url::Origin::Create(GURL(kURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_EQ(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kOtherURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(), GetCookieSettingOverrides(),
                maybe_included_cookies, excluded_cookies),
            IsTopLevel3pcdTrialEligible());

  if (IsTopLevel3pcdTrialEligible()) {
    EXPECT_THAT(maybe_included_cookies,
                ElementsAre(MatchesCookieWithAccessResult(
                    net::MatchesCookieWithName("third_party"),
                    MatchesCookieAccessResult(
                        AllOf(net::IsInclude(),
                              net::HasExactlyExemptionReason(
                                  net::CookieInclusionStatus::ExemptionReason::
                                      kTopLevel3PCDDeprecationTrial)),
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
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY,
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_USER_PREFERENCES}),
                    _, _, _))));
  }
}

TEST_P(CookieSettingsTopLevelTpcdTrialTest, PreserveBlockAllCookiesSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Set default cookie setting to block ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  // Add |TOP_LEVEL_TPCD_TRIAL| setting for |first_party_url|.
  settings.set_content_settings(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                                {CreateSettingForTopLevelTpcdTrial(
                                    top_level_url, CONTENT_SETTING_ALLOW)});

  base::HistogramTester histogram_tester;

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

TEST_P(CookieSettingsTopLevelTpcdTrialTest, PreserveExplicitBlock3pcSetting) {
  GURL first_party_url = GURL(kURL);
  GURL third_party_url = GURL(kOtherURL);
  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(true);

  // Precautionary - ensures that a default cookie setting is specified.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Explicitly block third-party cookies for resources embedded under
  // |first_party_url|.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", first_party_url.host(), CONTENT_SETTING_BLOCK)});

  // Add |TOP_LEVEL_TPCD_TRIAL| setting for |first_party_url|.
  settings.set_content_settings(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                                {CreateSettingForTopLevelTpcdTrial(
                                    first_party_url, CONTENT_SETTING_ALLOW)});

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(settings.GetCookieSetting(third_party_url, net::SiteForCookies(),
                                      first_party_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_BLOCKED, 1);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         CookieSettingsTopLevelTpcdTrialTest,
                         testing::Bool());

// The TOP_LEVEL_TPCD_ORIGIN_TRIAL content setting type is not registered on
// iOS.

#if !BUILDFLAG(IS_IOS)
class CookieSettingsTopLevelTpcdOriginTrialTest
    : public CookieSettingsTestBase,
      public testing::
          WithParamInterface</* net::features::kTopLevelTpcdOriginTrial:
                              */
                             bool> {
 public:
  CookieSettingsTopLevelTpcdOriginTrialTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTopLevel3pcdOriginTrialEligible()) {
      enabled_features.push_back(net::features::kTopLevelTpcdOriginTrial);
    } else {
      disabled_features.push_back(net::features::kTopLevelTpcdOriginTrial);
    }

    enabled_features.push_back(net::features::kTpcdMetadataGrants);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsTopLevel3pcdOriginTrialEligible() const { return GetParam(); }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    return overrides;
  }

  // The cookie access would be allowed if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting.
  ContentSetting SettingWithTopLevel3pcdOriginTrialSetting() const {
    return IsTopLevel3pcdOriginTrialEligible() ? CONTENT_SETTING_BLOCK
                                               : CONTENT_SETTING_ALLOW;
  }

  // The storage access result would be allowed if not for a
  // `ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting.
  net::cookie_util::StorageAccessResult
  AllowedStorageAccessResultWithTopLevel3pcdOriginTrialSetting() const {
    if (IsTopLevel3pcdOriginTrialEligible()) {
      return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED;
  }

  // The default scope for `ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL` is
  // `WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE`, so this adds a
  // `CONTENT_SETTING_BLOCK` setting with a pattern of that form to `settings`.
  void AddSettingForTopLevelTpcdOriginTrial(CookieSettings& settings,
                                            GURL top_level_url) {
    settings.set_content_settings(
        ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL,
        {ContentSettingPatternSource(
            ContentSettingsPattern::FromURLNoWildcard(top_level_url),
            ContentSettingsPattern::Wildcard(),
            base::Value(CONTENT_SETTING_BLOCK),
            content_settings::ProviderType::kNone, false /* incognito */)});
  }
};

// Verifies that `ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL` is
// allow-by-default and doesn't prevent cookie access.
TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest, AllowByDefault) {
  CookieSettings settings;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  EXPECT_EQ(settings.GetCookieSetting(GURL(kOtherURL), net::SiteForCookies(),
                                      GURL(kURL), net::CookieSettingOverrides(),
                                      nullptr),
            CONTENT_SETTING_ALLOW);

  net::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);

  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kOtherURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest, IsCookieAccessible) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  AddSettingForTopLevelTpcdOriginTrial(settings, GURL(kURL));

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);

  EXPECT_NE(settings.IsCookieAccessible(
                *cookie, GURL(kOtherURL), net::SiteForCookies(),
                url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
                GetCookieSettingOverrides(), &status),
            IsTopLevel3pcdOriginTrialEligible());
  // When active, the `TOP_LEVEL_TPCD_ORIGIN_TRIAL` should be blocking
  // third-party cookies, so the exemption reason should always be `kNone`.
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);
  EXPECT_EQ(status.HasExclusionReason(
                net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT),
            IsTopLevel3pcdOriginTrialEligible());
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest, UnblockedByMitigations) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  base::HistogramTester histogram_tester;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdOriginTrialSetting());

  // Create a metadata grant to allow cookies for `url` under `top_level_url`.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);
  net::CookieInclusionStatus status;

  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, url, net::SiteForCookies(), url::Origin::Create(top_level_url),
      net::FirstPartySetMetadata(), GetCookieSettingOverrides(), &status));
  EXPECT_EQ(status.exemption_reason() ==
                net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata,
            IsTopLevel3pcdOriginTrialEligible());

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD_METADATA_GRANT,
      IsTopLevel3pcdOriginTrialEligible() ? 1 : 0);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       BypassDefaultAllowAllCookiesSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  // Set default cookie setting to allow ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Add `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting for `top_level_url`.
  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  base::HistogramTester histogram_tester;

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            SettingWithTopLevel3pcdOriginTrialSetting());

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      AllowedStorageAccessResultWithTopLevel3pcdOriginTrialSetting(), 1);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       PreserveExplicitAllow3pcSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(false);

  // Explicitly allow third-party cookies for resources embedded under
  // `top_level_url`.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", top_level_url.host(), CONTENT_SETTING_ALLOW)});

  // Add `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting for `top_level_url`.
  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);

  histogram_tester.ExpectUniqueSample(
      kAllowedRequestsHistogram,
      net::cookie_util::StorageAccessResult::ACCESS_ALLOWED, 1);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);
  net::CookieInclusionStatus status;

  EXPECT_TRUE(settings.IsCookieAccessible(
      *cookie, GURL(kOtherURL), net::SiteForCookies(),
      url::Origin::Create(GURL(kURL)), net::FirstPartySetMetadata(),
      GetCookieSettingOverrides(), &status));
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kUserSetting);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       PreserveExplicitBlock3pcSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);
  base::HistogramTester histogram_tester;

  CookieSettings settings;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  // Set default cookie setting to allow ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Explicitly block third-party cookies for resources embedded under
  // `top_level_url`.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", top_level_url.host(), CONTENT_SETTING_BLOCK)});

  // Check that 3P cookies are blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Add `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting for `top_level_url`.
  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  // Create a metadata grant to allow cookies for `url` under
  // `top_level_url`.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);
  net::CookieInclusionStatus status;

  // Check that 3P cookies are still blocked for `url` under `top_level_url`.
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, url, net::SiteForCookies(), url::Origin::Create(top_level_url),
      net::FirstPartySetMetadata(), GetCookieSettingOverrides(), &status));
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       PreserveBlockAll3PCookiesSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(true);
  settings.set_mitigations_enabled_for_3pcd(false);

  // Set default cookie setting to allow ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});

  // Check that 3P cookies are blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Add `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting for `top_level_url`.
  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  // Create a metadata grant to allow cookies for `url` under
  // `top_level_url`.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);
  net::CookieInclusionStatus status;

  // Check that 3P cookies are still blocked for `url` under `top_level_url`.
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, url, net::SiteForCookies(), url::Origin::Create(top_level_url),
      net::FirstPartySetMetadata(), GetCookieSettingOverrides(), &status));
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       PreserveBlockAllCookiesSetting) {
  GURL top_level_url = GURL(kURL);
  GURL url = GURL(kOtherURL);

  CookieSettings settings;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  // Set default cookie setting to block ALL cookies.
  settings.set_content_settings(
      ContentSettingsType::COOKIES,
      {CreateSetting("*", "*", CONTENT_SETTING_BLOCK)});

  // Check that 3P cookies are blocked.
  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);

  // Add `TOP_LEVEL_TPCD_ORIGIN_TRIAL` setting for `top_level_url`.
  AddSettingForTopLevelTpcdOriginTrial(settings, top_level_url);

  // Create a metadata grant to allow cookies for `url` under
  // `top_level_url`.
  network::tpcd::metadata::Manager manager;
  manager.SetGrants(
      {CreateSetting(url.host(), top_level_url.host(), CONTENT_SETTING_ALLOW)});
  settings.set_tpcd_metadata_manager(&manager);

  std::unique_ptr<net::CanonicalCookie> cookie =
      MakeCanonicalSameSiteNoneCookie("name", kOtherURL);
  net::CookieInclusionStatus status;

  // Check that cookies are still blocked for `url` under `top_level_url`.
  EXPECT_FALSE(settings.IsCookieAccessible(
      *cookie, url, net::SiteForCookies(), url::Origin::Create(top_level_url),
      net::FirstPartySetMetadata(), GetCookieSettingOverrides(), &status));
  EXPECT_EQ(status.exemption_reason(),
            net::CookieInclusionStatus::ExemptionReason::kNone);

  EXPECT_EQ(settings.GetCookieSetting(url, net::SiteForCookies(), top_level_url,
                                      GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

TEST_P(CookieSettingsTopLevelTpcdOriginTrialTest,
       AnnotateAndMoveUserBlockedCookies) {
  CookieSettings settings;
  net::CookieInclusionStatus status;
  settings.set_block_third_party_cookies(false);
  settings.set_mitigations_enabled_for_3pcd(false);

  AddSettingForTopLevelTpcdOriginTrial(settings, GURL(kURL));

  net::CookieAccessResultList maybe_included_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("third_party", kOtherURL), {}},
  };
  net::CookieAccessResultList excluded_cookies = {
      {*MakeCanonicalSameSiteNoneCookie("excluded_other", kOtherURL),
       // The `ExclusionReason` below is irrelevant, as long as there is one.
       net::CookieAccessResult(net::CookieInclusionStatus(
           net::CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY))}};

  url::Origin origin = url::Origin::Create(GURL(kURL));

  // Note that `url` does not match the `top_frame_origin`.
  EXPECT_NE(settings.AnnotateAndMoveUserBlockedCookies(
                GURL(kOtherURL), net::SiteForCookies(), &origin,
                net::FirstPartySetMetadata(), GetCookieSettingOverrides(),
                maybe_included_cookies, excluded_cookies),
            IsTopLevel3pcdOriginTrialEligible());

  if (IsTopLevel3pcdOriginTrialEligible()) {
    EXPECT_THAT(maybe_included_cookies, IsEmpty());
    EXPECT_THAT(
        excluded_cookies,
        UnorderedElementsAre(
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("excluded_other"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::ExclusionReason::
                                EXCLUDE_SECURE_ONLY}),
                    _, _, _)),
            MatchesCookieWithAccessResult(
                net::MatchesCookieWithName("third_party"),
                MatchesCookieAccessResult(
                    HasExactlyExclusionReasonsForTesting(
                        std::vector<
                            net::CookieInclusionStatus::ExclusionReason>{
                            net::CookieInclusionStatus::
                                EXCLUDE_THIRD_PARTY_PHASEOUT}),
                    _, _, _))));
  } else {
    EXPECT_THAT(
        maybe_included_cookies,
        ElementsAre(MatchesCookieWithAccessResult(
            net::MatchesCookieWithName("third_party"),
            MatchesCookieAccessResult(
                AllOf(net::IsInclude(),
                      net::HasExactlyExemptionReason(
                          net::CookieInclusionStatus::ExemptionReason::kNone)),
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
  }
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         CookieSettingsTopLevelTpcdOriginTrialTest,
                         testing::Bool());
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace network
