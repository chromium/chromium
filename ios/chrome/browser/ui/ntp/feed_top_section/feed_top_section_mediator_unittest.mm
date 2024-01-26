// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator+testing.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mutator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class FeedTopSectionMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    fake_browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        fake_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    fake_authentication_service_ = GetAuthenticationService();
    fake_pref_service_ = fake_browser_state_->GetPrefs();
    feature_list_.InitAndEnableFeatureWithParameters(
        kContentPushNotifications,
        {{kContentPushNotificationsExperimentType, "1"}});
    feed_top_section_view_controller_ =
        [[FeedTopSectionViewController alloc] init];
    feed_top_section_mediator_ = [[FeedTopSectionMediator alloc]
        initWithConsumer:[[FeedTopSectionViewController alloc] init]
         identityManager:IdentityManagerFactory::GetForBrowserState(
                             fake_browser_state_.get())
             authService:fake_authentication_service_
             isIncognito:fake_browser_state_.get()->IsOffTheRecord()
             prefService:fake_pref_service_];
    feed_top_section_view_controller_.feedTopSectionMutator =
        feed_top_section_mediator_;
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForBrowserState(
        fake_browser_state_.get());
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  AuthenticationService* fake_authentication_service_;
  PrefService* fake_pref_service_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> fake_browser_state_;
  FeedTopSectionMediator* feed_top_section_mediator_;
  FeedTopSectionViewController* feed_top_section_view_controller_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

#pragma mark - Notifications Promo Action Tests

TEST_F(FeedTopSectionMediatorTest,
       TestNotificationsActionClosePromoFromCloseButton) {
  histogram_tester_->ExpectBucketCount(
      "ContentNotifications.Promo.TopOfFeed.Action",
      ContentNotificationTopOfFeedPromoAction::kDismissedFromCloseButton, 0);
  [feed_top_section_view_controller_.feedTopSectionMutator
      notificationsPromoViewDismissedFromButton:
          NotificationsPromoButtonTypeClose];
  histogram_tester_->ExpectBucketCount(
      "ContentNotifications.Promo.TopOfFeed.Action",
      ContentNotificationTopOfFeedPromoAction::kDismissedFromCloseButton, 1);
}

TEST_F(FeedTopSectionMediatorTest,
       TestNotificationsActionClosePromoFromSecondaryButton) {
  histogram_tester_->ExpectBucketCount(
      "ContentNotifications.Promo.TopOfFeed.Action",
      ContentNotificationTopOfFeedPromoAction::kDismissedFromSecondaryButton,
      0);
  [feed_top_section_view_controller_.feedTopSectionMutator
      notificationsPromoViewDismissedFromButton:
          NotificationsPromoButtonTypeSecondary];
  histogram_tester_->ExpectBucketCount(
      "ContentNotifications.Promo.TopOfFeed.Action",
      ContentNotificationTopOfFeedPromoAction::kDismissedFromSecondaryButton,
      1);
}
