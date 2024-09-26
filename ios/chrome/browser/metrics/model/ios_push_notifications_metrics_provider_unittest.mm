// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_push_notifications_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Installs a mock PushNotificationUtil whose -getPermissionSettings: method
// will invoke the block passed with a setting with the given `status`. The
// mock will be uninstalled when the returned object is destroyed.
id InstallMockPushNotificationUtil(UNAuthorizationStatus status) {
  id mock = OCMClassMock([PushNotificationUtil class]);
  OCMStub(ClassMethod([mock getPermissionSettings:[OCMArg any]]))
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained void (^block)(UNNotificationSettings*) = nil;
        [invocation getArgument:&block atIndex:2];
        if (block) {
          id mock_value = OCMClassMock([UNNotificationSettings class]);
          OCMStub([mock_value authorizationStatus]).andReturn(status);
          block(mock_value);
        }
      });
  return mock;
}

}  // namespace

// Tests metrics that are recorded and uploaded by
// IOSPushNotificationsMetricsProvider.
class IOSPushNotificationsMetricsProviderTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_push_notification_util_ =
        InstallMockPushNotificationUtil(UNAuthorizationStatusAuthorized);
  }

  void TearDown() override {
    mock_push_notification_util_ = nil;
    PlatformTest::TearDown();
  }

  // Creates a TestProfileIOS and pretends it is signed-in with
  // `identity` if not nil.
  void AddProfile(const std::string& name, FakeSystemIdentity* identity) {
    TestProfileIOS::Builder builder;
    builder.SetName(name);
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile, std::make_unique<FakeAuthenticationServiceDelegate>());

    if (identity) {
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager())
          ->AddIdentity(identity);

      AuthenticationServiceFactory::GetForProfile(profile)->SignIn(
          identity, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
    }
  }

  void AddProfile(const std::string& name) {
    AddProfile(name, /*identity=*/nil);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::HistogramTester histogram_tester_;
  id mock_push_notification_util_ = nil;
};

// Tests that ProvideCurrentSessionData(...) records the status of the
// Push notification autorization and no other data when there are no
// ProfileIOS loaded.
TEST_F(IOSPushNotificationsMetricsProviderTest,
       ProvideCurrentSessionData_NoBrowserState) {
  IOSPushNotificationsMetricsProvider provider;

  // Record the metrics without any BrowserStates.
  provider.ProvideCurrentSessionData(/*uma_proto*/ nullptr);

  // The status of the notification authorization must always be logged
  // exactly once (this is per device, not per BrowserState).
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kNotifAuthorizationStatusByProviderHistogram),
      ::testing::ElementsAre(base::Bucket(UNAuthorizationStatusAuthorized, 1)));

  // The per-BrowserState metrics must not be recorded if there are no
  // BrowserState loaded.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kContentNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSportsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kTipsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSafetyCheckNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSendTabNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());
}

// Tests that ProvideCurrentSessionData(...) records the status of the
// Push notification autorization and the data for each notification
// client for the loaded ProfileIOS.
TEST_F(IOSPushNotificationsMetricsProviderTest,
       ProvideCurrentSessionData_OneBrowserState) {
  IOSPushNotificationsMetricsProvider provider;

  // Add one BrowserState and record the metrics.
  AddProfile("Default");
  provider.ProvideCurrentSessionData(/*uma_proto*/ nullptr);

  // The status of the notification authorization must always be logged
  // exactly once (this is per device, not per BrowserState).
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kNotifAuthorizationStatusByProviderHistogram),
      ::testing::ElementsAre(base::Bucket(UNAuthorizationStatusAuthorized, 1)));

  // The per-BrowserState metrics must be recorded for the loaded
  // BrowserState (but only for the metrics that do not require a
  // signed-in identity since the BrowserState is not signed-in).
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kContentNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSportsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kTipsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSafetyCheckNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSendTabNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre());
}

// Tests that ProvideCurrentSessionData(...) records the status of the
// Push notification autorization and the data for each notification
// client for the loaded ProfileIOS.
TEST_F(IOSPushNotificationsMetricsProviderTest,
       ProvideCurrentSessionData_MultipleBrowserStates) {
  IOSPushNotificationsMetricsProvider provider;

  // Add a few BrowserStates and record the metrics.
  AddProfile("Profile1");
  AddProfile("Profile2");
  AddProfile("Profile3", [FakeSystemIdentity fakeIdentity1]);
  provider.ProvideCurrentSessionData(/*uma_proto*/ nullptr);

  // The status of the notification authorization must always be logged
  // exactly once (this is per device, not per BrowserState).
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          kNotifAuthorizationStatusByProviderHistogram),
      ::testing::ElementsAre(base::Bucket(UNAuthorizationStatusAuthorized, 1)));

  // The per-BrowserState metrics must be recorded for each loaded
  // BrowserStates (though the metrics that requires a signed-in
  // identity will only be logged for "Profile3").
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kContentNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSportsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kTipsNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 3)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSafetyCheckNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 3)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kSendTabNotifClientStatusByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(0, 1)));
}
