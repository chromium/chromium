// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
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
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

void ExpectTotalCountForTriggerCriteriaExperiment(
    base::HistogramTester* histogram_tester,
    const std::string& action_str,
    int count) {
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".PromoDisplayCount", count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".LastPromoInteractionNumDays",
      count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".ChromeColdStartCount", count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".ChromeWarmStartCount", count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".ChromeIndirectStartCount",
      count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".PasswordManagerUseCount",
      count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".OmniboxClipboardUseCount",
      count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".BookmarkUseCount", count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".AutofllUseCount", count);
  histogram_tester->ExpectTotalCount(
      "IOS.DefaultBrowserPromo." + action_str + ".SpecialTabsUseCount", count);
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

  base::test::TaskEnvironment task_environment_;
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
               kConfirmationAlertTertiaryActionAccessibilityIdentifier);
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

// Tests that the right histograms are recorded for trigger criteria experiment.
TEST_F(DefaultBrowserGenericPromoCoordinatorTest,
       TestTriggerCriteriaExperimentCancelAction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feature_engagement::kDefaultBrowserTriggerCriteriaExperiment);
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  // Check that histograms for appear action are recorded, but for other actions
  // there are not.
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Appear", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester,
                                               "PrimaryAction", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Cancel", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Dismiss", 0);

  DefaultBrowserInstructionsViewController* promo_view_controller =
      base::apple::ObjCCastStrict<DefaultBrowserInstructionsViewController>(
          view_controller_.presentedViewController);

  UIView* secondary_button_view =
      FindByID(promo_view_controller.view,
               kConfirmationAlertSecondaryActionAccessibilityIdentifier);
  EXPECT_NSNE(nil, secondary_button_view);
  ASSERT_TRUE([secondary_button_view isKindOfClass:[UIButton class]]);

  UIButton* secondary_button =
      base::apple::ObjCCastStrict<UIButton>(secondary_button_view);
  [secondary_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  // Check that after dismissing the promo only histograms for cancel action are
  // recorded.
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Appear", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester,
                                               "PrimaryAction", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Cancel", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Dismiss", 0);
}

// Tests that the right histograms are recorded for trigger criteria experiment.
TEST_F(DefaultBrowserGenericPromoCoordinatorTest,
       TestTriggerCriteriaExperimentPrimaryAction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      feature_engagement::kDefaultBrowserTriggerCriteriaExperiment);
  base::HistogramTester histogram_tester;

  [coordinator_ start];

  // Mock the mediator which otherwise will open the iOS settings on primary
  // action. This can be a problem for next tests.
  id mock_mediator = OCMClassMock([DefaultBrowserGenericPromoMediator class]);
  coordinator_.mediator = mock_mediator;

  // Check that histograms for appear action are recorded, but for other actions
  // there are not.
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Appear", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester,
                                               "PrimaryAction", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Cancel", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Dismiss", 0);

  DefaultBrowserInstructionsViewController* promo_view_controller =
      base::apple::ObjCCastStrict<DefaultBrowserInstructionsViewController>(
          view_controller_.presentedViewController);
  UIView* primary_button_view =
      FindByID(promo_view_controller.view,
               kConfirmationAlertPrimaryActionAccessibilityIdentifier);
  EXPECT_NSNE(nil, primary_button_view);
  ASSERT_TRUE([primary_button_view isKindOfClass:[UIButton class]]);

  UIButton* primary_button =
      base::apple::ObjCCastStrict<UIButton>(primary_button_view);
  [primary_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  // Check that after primaryaction the promo only histograms for primary action
  // action are recorded.
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Appear", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester,
                                               "PrimaryAction", 1);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Cancel", 0);
  ExpectTotalCountForTriggerCriteriaExperiment(&histogram_tester, "Dismiss", 0);
}
