// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#import "ios/chrome/browser/enterprise/connectors/features.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_enterprise_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace safe_browsing {

class SafeBrowsingClientFactoryTest : public PlatformTest {
 protected:
  SafeBrowsingClientFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
};

// Checks that different instances are returned for recording and off the record
// profiles.
TEST_F(SafeBrowsingClientFactoryTest, DifferentClientInstances) {
  SafeBrowsingClient* recording_client =
      SafeBrowsingClientFactory::GetForProfile(profile_.get());
  SafeBrowsingClient* off_the_record_client =
      SafeBrowsingClientFactory::GetForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(recording_client);
  EXPECT_TRUE(off_the_record_client);
  EXPECT_NE(recording_client, off_the_record_client);
}

// Tests that SafeBrowsingClientFactory returns the enterprise url lookup
// service when Url filtering is enabled.
TEST_F(SafeBrowsingClientFactoryTest, GetEnterpriseOrConsumerLookupService) {
  base::test::ScopedFeatureList feature(
      enterprise_connectors::kIOSEnterpriseRealtimeUrlFiltering);

  SafeBrowsingClient* recording_client =
      SafeBrowsingClientFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(recording_client);

  // Consumer service should be used when enterprise policy is disabled.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_DISABLED);

  RealTimeUrlLookupServiceBase* url_lookup_service =
      recording_client->GetRealTimeUrlLookupService();
  RealTimeUrlLookupServiceBase* consumer_url_lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(profile_.get());

  EXPECT_EQ(url_lookup_service, consumer_url_lookup_service);

  // Enterprise service should be used when the policy is enabled.
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
      enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  // Simulating Enterprise Url filtering enabled requires a Dm token. Setting a
  // test one.
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage;
  fake_browser_dm_token_storage.SetDMToken("dm_token");
  fake_browser_dm_token_storage.SetClientId("client_id");

  url_lookup_service = recording_client->GetRealTimeUrlLookupService();
  RealTimeUrlLookupServiceBase* enterprise_url_lookup_service =
      ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
          profile_.get());

  EXPECT_NE(consumer_url_lookup_service, enterprise_url_lookup_service);
  EXPECT_EQ(url_lookup_service, enterprise_url_lookup_service);
}

}  // namespace safe_browsing
