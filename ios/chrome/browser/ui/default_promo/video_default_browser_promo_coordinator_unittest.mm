// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Create the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace

#pragma mark - Fixture.

// Fixture to test VideoDefaultBrowserPromoCoordinator.
class VideoDefaultBrowserPromoCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserState(
            browser_state_.get()));

    coordinator_ = [[VideoDefaultBrowserPromoCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  VideoDefaultBrowserPromoCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  feature_engagement::test::MockTracker* mock_tracker_;
};

#pragma mark - Tests.

// Tests that the proper metrics and FET events are fired when tapping Remind Me
// Later.
TEST_F(VideoDefaultBrowserPromoCoordinatorTest, TestRemindMeLater) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_NSEQ(nil, view_controller_.presentedViewController);

  coordinator_.showRemindMeLater = YES;
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(feature_engagement::events::kDefaultBrowserPromoShown));
  [coordinator_ start];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserVideoPromo.Appear"));

  // This should present a VideoDefaultBrowserPromoViewController.
  ASSERT_TRUE([view_controller_.presentedViewController
      isKindOfClass:[VideoDefaultBrowserPromoViewController class]]);
  VideoDefaultBrowserPromoViewController* video_promo_view_controller =
      base::apple::ObjCCastStrict<VideoDefaultBrowserPromoViewController>(
          view_controller_.presentedViewController);

  EXPECT_EQ(YES, video_promo_view_controller.showRemindMeLater);

  // Prepare to tap tertiary action.
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kDefaultBrowserPromoRemindMeLater));

  [video_promo_view_controller.actionHandler confirmationAlertTertiaryAction];

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserVideoPromo.Fullscreen.RemindMeLater"));
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                     3, 1);
}
