// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/private_insights/model/private_insights_service_factory.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/private_insights/private_insights_features.h"
#import "components/private_insights/private_insights_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using private_insights::kPrivateInsightsFeature;
using private_insights::kPrivateInsightsUploadInterval;
using private_insights::kTriggerUploadOutcomeHistogram;
using private_insights::PrivateInsightsMetricsServiceAccessor;
using private_insights::PrivateInsightsService;

}  // namespace

class PrivateInsightsServiceFactoryTest : public PlatformTest {
 protected:
  PrivateInsightsServiceFactoryTest() {
    // Non-official builds ignore the UMA pref unless this is set.
    PrivateInsightsMetricsServiceAccessor::
        SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);
  }

  ~PrivateInsightsServiceFactoryTest() override {
    PrivateInsightsMetricsServiceAccessor::
        SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
  }

  // Builds a regular profile that creates `PrivateInsightsService` with the
  // production factory. Testing profiles do not get the service by default.
  std::unique_ptr<TestProfileIOS> BuildProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        PrivateInsightsServiceFactory::GetInstance(),
        PrivateInsightsServiceFactory::GetDefaultFactory());
    return std::move(builder).Build();
  }

  void SetMetricsReportingEnabled(bool enabled) {
    GetApplicationContext()->GetLocalState()->SetBoolean(
        metrics::prefs::kMetricsReportingEnabled, enabled);
  }

  // Advances mock time by one upload interval, which fires the upload timer
  // once if the service is started.
  void FastForwardByUploadInterval() {
    task_environment_.FastForwardBy(kPrivateInsightsUploadInterval.Get());
  }

  // Expects the upload timer to have fired `count` times in total. Without
  // queued events each firing records only `kSkippedNoData`.
  void ExpectUploadTimerFiredCount(int count) const {
    histogram_tester_.ExpectBucketCount(
        kTriggerUploadOutcomeHistogram,
        PrivateInsightsService::TriggerUploadOutcome::kSkippedNoData, count);
    histogram_tester_.ExpectTotalCount(kTriggerUploadOutcomeHistogram, count);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{kPrivateInsightsFeature};
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  // `PrivateInsightsService` CHECKs that local state exists.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::HistogramTester histogram_tester_;
};

// Tests that the service is created when `kPrivateInsightsFeature` is enabled.
TEST_F(PrivateInsightsServiceFactoryTest, ServiceCreatedWhenFeatureEnabled) {
  std::unique_ptr<TestProfileIOS> profile = BuildProfile();

  EXPECT_NE(PrivateInsightsServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

// Tests that no service is created when `kPrivateInsightsFeature` is disabled.
TEST_F(PrivateInsightsServiceFactoryTest, ServiceNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(kPrivateInsightsFeature);
  std::unique_ptr<TestProfileIOS> profile = BuildProfile();

  EXPECT_EQ(PrivateInsightsServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

// Tests that incognito profiles never get a service, even with the production
// factory installed.
TEST_F(PrivateInsightsServiceFactoryTest, ServiceNullForIncognito) {
  std::unique_ptr<TestProfileIOS> profile = BuildProfile();
  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              PrivateInsightsServiceFactory::GetInstance(),
              PrivateInsightsServiceFactory::GetDefaultFactory()}});
  ASSERT_TRUE(otr_profile);

  EXPECT_EQ(PrivateInsightsServiceFactory::GetForProfile(otr_profile), nullptr);
}

// Tests that each profile gets its own service, so profiles never share
// queued events or contribution history.
TEST_F(PrivateInsightsServiceFactoryTest, SeparateServicePerProfile) {
  std::unique_ptr<TestProfileIOS> profile1 = BuildProfile();
  std::unique_ptr<TestProfileIOS> profile2 = BuildProfile();

  PrivateInsightsService* service1 =
      PrivateInsightsServiceFactory::GetForProfile(profile1.get());
  PrivateInsightsService* service2 =
      PrivateInsightsServiceFactory::GetForProfile(profile2.get());
  ASSERT_TRUE(service1);
  ASSERT_TRUE(service2);
  EXPECT_NE(service1, service2);
}

// Tests that the upload timer starts and stops with the UMA consent.
TEST_F(PrivateInsightsServiceFactoryTest, UploadsFollowMetricsConsent) {
  SetMetricsReportingEnabled(false);
  std::unique_ptr<TestProfileIOS> profile = BuildProfile();
  ASSERT_TRUE(PrivateInsightsServiceFactory::GetForProfile(profile.get()));

  FastForwardByUploadInterval();
  ExpectUploadTimerFiredCount(0);

  SetMetricsReportingEnabled(true);
  FastForwardByUploadInterval();
  ExpectUploadTimerFiredCount(1);

  SetMetricsReportingEnabled(false);
  FastForwardByUploadInterval();
  ExpectUploadTimerFiredCount(1);
}

// Tests that the service is created with the profile, without any
// `GetForProfile()` call, and starts uploading right away when UMA consent is
// already granted, like on desktop.
TEST_F(PrivateInsightsServiceFactoryTest, ServiceCreatedWithProfile) {
  SetMetricsReportingEnabled(true);
  std::unique_ptr<TestProfileIOS> profile = BuildProfile();

  EXPECT_NE(PrivateInsightsServiceFactory::GetForProfileIfExists(profile.get()),
            nullptr);

  // Only a service built and initialized with the profile runs the timer.
  FastForwardByUploadInterval();
  ExpectUploadTimerFiredCount(1);
}
