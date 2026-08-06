// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/non_modal/coordinator/default_browser_promo_non_modal_coordinator.h"

#import "base/strings/string_number_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface DefaultBrowserPromoNonModalCoordinator (Testing)
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@end

@interface InfobarBannerViewController (Testing)
@property(nonatomic, copy, readonly) NSString* titleText;
@property(nonatomic, copy, readonly) NSString* subtitleText;
@end

class DefaultBrowserPromoNonModalCoordinatorTest : public PlatformTest {
 protected:
  DefaultBrowserPromoNonModalCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* view_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the default strings are used when the experiment feature is
// disabled.
TEST_F(DefaultBrowserPromoNonModalCoordinatorTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kOmniboxPastePromoExperiment);

  DefaultBrowserPromoNonModalCoordinator* coordinator =
      [[DefaultBrowserPromoNonModalCoordinator alloc]
          initWithBaseViewController:view_controller_
                             browser:browser_.get()
                         promoReason:NonModalDefaultBrowserPromoReason::
                                         PromoReasonOmniboxPaste];
  [coordinator start];

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE);
  NSString* expected_subtitle = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION);

  EXPECT_NSEQ(expected_title, coordinator.bannerViewController.titleText);
  EXPECT_NSEQ(expected_subtitle, coordinator.bannerViewController.subtitleText);
}

// Tests that the default strings are used when the experiment feature is
// enabled but no arm is specified.
TEST_F(DefaultBrowserPromoNonModalCoordinatorTest, FeatureEnabledNoArm) {
  scoped_feature_list_.InitAndEnableFeature(kOmniboxPastePromoExperiment);

  DefaultBrowserPromoNonModalCoordinator* coordinator =
      [[DefaultBrowserPromoNonModalCoordinator alloc]
          initWithBaseViewController:view_controller_
                             browser:browser_.get()
                         promoReason:NonModalDefaultBrowserPromoReason::
                                         PromoReasonOmniboxPaste];
  [coordinator start];

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP1);
  NSString* expected_subtitle = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP1);

  EXPECT_NSEQ(expected_title, coordinator.bannerViewController.titleText);
  EXPECT_NSEQ(expected_subtitle, coordinator.bannerViewController.subtitleText);
}

// Tests that the correct strings are used for each experiment arm.
TEST_F(DefaultBrowserPromoNonModalCoordinatorTest, ExperimentArms) {
  struct {
    std::string arm_value;
    int title_id;
    int subtitle_id;
  } kTestCases[] = {
      {"1", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP1,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP1},
      {"2", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP2,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP2},
      {"3", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP3,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP3},
      {"4", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP4,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP4},
      {"5", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP5,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP5},
      {"6", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP6,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP6},
      {"7", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP7,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP7},
      {"8", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP8,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP8},
      {"9", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP9,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP9},
      {"10", IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE_EXP10,
       IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION_EXP10},
  };

  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList local_feature_list;
    local_feature_list.InitAndEnableFeatureWithParameters(
        kOmniboxPastePromoExperiment, {{"arm", test_case.arm_value}});

    DefaultBrowserPromoNonModalCoordinator* coordinator =
        [[DefaultBrowserPromoNonModalCoordinator alloc]
            initWithBaseViewController:view_controller_
                               browser:browser_.get()
                           promoReason:NonModalDefaultBrowserPromoReason::
                                           PromoReasonOmniboxPaste];
    [coordinator start];

    NSString* expected_title = l10n_util::GetNSString(test_case.title_id);
    NSString* expected_subtitle = l10n_util::GetNSString(test_case.subtitle_id);

    EXPECT_NSEQ(expected_title, coordinator.bannerViewController.titleText);
    EXPECT_NSEQ(expected_subtitle,
                coordinator.bannerViewController.subtitleText);
  }
}
