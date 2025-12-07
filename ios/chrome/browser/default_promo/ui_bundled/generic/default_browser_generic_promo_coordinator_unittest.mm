// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Finds a subview with a given accessibility id.
UIView* FindByID(UIView* view, NSString* accessibility_id) {
  if (view.accessibilityIdentifier == accessibility_id) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView = FindByID(subview, accessibility_id);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

// Create the Feature Engagement Mock Tracker.
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    ProfileIOS* profile) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

}  // namespace

#pragma mark - Fixture.

// Fixture to test DefaultBrowserGenericPromoCoordinator.
class DefaultBrowserGenericPromoCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];

    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    coordinator_ = [[DefaultBrowserGenericPromoCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  DefaultBrowserGenericPromoCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;
};

#pragma mark - Tests.

// Tests that the proper metrics and FET events are fired when tapping Remind Me
// Later.
TEST_F(DefaultBrowserGenericPromoCoordinatorTest, TestRemindMeLater) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature}, {});
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  EXPECT_NSEQ(nil, view_controller_.presentedViewController);

  coordinator_.promoWasFromRemindMeLater = NO;
  [coordinator_ start];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserVideoPromo.Appear"));

  // This should present a DefaultBrowserInstructionsViewController.
  ASSERT_TRUE([view_controller_.presentedViewController
      isKindOfClass:[DefaultBrowserInstructionsViewController class]]);
  DefaultBrowserInstructionsViewController* promo_view_controller =
      base::apple::ObjCCastStrict<DefaultBrowserInstructionsViewController>(
          view_controller_.presentedViewController);

  UIView* tertiary_button_view =
      FindByID(promo_view_controller.view,
               kButtonStackTertiaryActionAccessibilityIdentifier);
  EXPECT_NSNE(nil, tertiary_button_view);
  ASSERT_TRUE([tertiary_button_view isKindOfClass:[UIButton class]]);

  UIButton* tertiary_button =
      base::apple::ObjCCastStrict<UIButton>(tertiary_button_view);

  // Prepare to tap tertiary action.
  EXPECT_CALL(
      *mock_tracker_,
      NotifyEvent(
          feature_engagement::events::kDefaultBrowserPromoRemindMeLater));

  [tertiary_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserVideoPromo.Fullscreen.RemindMeLater"));
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                     3, 1);
}

// Tests that when the promo is persistent (doesn't dismiss on primary button
// tap), only the first action causes metrics to be recorded.
TEST_F(DefaultBrowserGenericPromoCoordinatorTest,
       TestPersistentPromoOnlyRecordsOutcomeOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPersistentDefaultBrowserPromo);
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  // Mock the mediator which otherwise will open the iOS settings on primary
  // action. This can be a problem for next tests.
  id mock_mediator = OCMClassMock([DefaultBrowserGenericPromoMediator class]);
  coordinator_.mediator = mock_mediator;

  // Check that only histograms for appear action are recorded.
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserPromo.Shown", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserPromo.Shown",
                                     DefaultPromoTypeForUMA::kGeneral, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                    0);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserVideoPromo.PersistedDuration", 0);

  // Tap the primary button.
  UINavigationController* navigation_controller =
      base::apple::ObjCCastStrict<UINavigationController>(
          view_controller_.presentedViewController);
  DefaultBrowserInstructionsViewController* promo_view_controller =
      base::apple::ObjCCastStrict<DefaultBrowserInstructionsViewController>(
          navigation_controller.topViewController);
  UIView* primary_button_view =
      FindByID(promo_view_controller.view,
               kButtonStackPrimaryActionAccessibilityIdentifier);
  EXPECT_NSNE(nil, primary_button_view);
  ASSERT_TRUE([primary_button_view isKindOfClass:[UIButton class]]);

  UIButton* primary_button =
      base::apple::ObjCCastStrict<UIButton>(primary_button_view);
  [primary_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  // Check that the primary button tap has been recorded.
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserPromo.Shown", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserPromo.Shown",
                                     DefaultPromoTypeForUMA::kGeneral, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                    1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserPromo.Shown",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserVideoPromo.PersistedDuration", 0);

  // Tap the secondary button to dismiss the promo.
  UIView* secondary_button_view =
      FindByID(promo_view_controller.view,
               kButtonStackSecondaryActionAccessibilityIdentifier);
  EXPECT_NSNE(nil, secondary_button_view);
  ASSERT_TRUE([secondary_button_view isKindOfClass:[UIButton class]]);

  UIButton* secondary_button =
      base::apple::ObjCCastStrict<UIButton>(secondary_button_view);
  [secondary_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  // Check that the metrics didn't change.
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserPromo.Shown", 1);
  histogram_tester.ExpectBucketCount("IOS.DefaultBrowserPromo.Shown",
                                     DefaultPromoTypeForUMA::kGeneral, 1);
  histogram_tester.ExpectTotalCount("IOS.DefaultBrowserVideoPromo.Fullscreen",
                                    1);
  histogram_tester.ExpectBucketCount(
      "IOS.DefaultBrowserPromo.Shown",
      IOSDefaultBrowserVideoPromoAction::kPrimaryActionTapped, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.DefaultBrowserVideoPromo.PersistedDuration", 1);
}
