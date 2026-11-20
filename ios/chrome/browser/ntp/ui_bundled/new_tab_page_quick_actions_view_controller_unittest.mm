// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/string_number_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using chrome_test_util::FindViewByClass;
using chrome_test_util::FindViewById;

// Test fixture for `NewTabPageQuickActionsViewController`.
class NewTabPageQuickActionsViewControllerTest : public PlatformTest {
 protected:
  NewTabPageQuickActionsViewController* CreateAndConfigureViewController() {
    NewTabPageQuickActionsViewController* view_controller =
        [[NewTabPageQuickActionsViewController alloc] init];
    [view_controller setAIMTitle:@"AI Mode"
                            icon:[[UIImage alloc] init]
              accessibilityLabel:@"AI Mode"];
    return view_controller;
  }
};

// Tests that the incognito button is visible by default and hidden when
// `setIncognitoAllowed:NO` is called after loading the view.
TEST_F(NewTabPageQuickActionsViewControllerTest,
       HidesIncognitoButtonWhenIncognitoDisallowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kAIMNTPEntrypointTablet},
      /*disabled_features=*/{kAimButtonRefactor});

  NewTabPageQuickActionsViewController* view_controller =
      CreateAndConfigureViewController();
  [view_controller loadViewIfNeeded];

  UIButton* aim_button = FindViewById<UIButton>(view_controller.view,
                                                kNTPAIMQuickActionIdentifier);
  UIButton* incognito_button = FindViewById<UIButton>(
      view_controller.view, kNTPIncognitoQuickActionIdentifier);
  ASSERT_NE(nil, aim_button);
  ASSERT_NE(nil, incognito_button);
  EXPECT_FALSE(aim_button.hidden);
  EXPECT_FALSE(incognito_button.hidden);

  // Disabling incognito dynamically hides only the incognito button.
  [view_controller setIncognitoAllowed:NO];
  EXPECT_FALSE(aim_button.hidden);
  EXPECT_TRUE(incognito_button.hidden);

  // Re-enabling incognito shows the button again.
  [view_controller setIncognitoAllowed:YES];
  EXPECT_FALSE(aim_button.hidden);
  EXPECT_FALSE(incognito_button.hidden);
}

// Tests that calling `setIncognitoAllowed:NO` before `viewDidLoad` keeps the
// incognito button hidden in multi-chip `kAimButtonRefactor` arms as well.
TEST_F(NewTabPageQuickActionsViewControllerTest,
       HidesIncognitoButtonBeforeViewLoadsInMultiChipArm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kAimButtonRefactor,
        {{kAimButtonRefactorArmParam,
          base::NumberToString(static_cast<int>(
              AimButtonRefactorArm::kImageGenerationQuickAction))}}},
       {kAIMNTPEntrypointTablet, {}}},
      /*disabled_features=*/{});

  NewTabPageQuickActionsViewController* view_controller =
      CreateAndConfigureViewController();
  [view_controller setIncognitoAllowed:NO];
  [view_controller setAIMImageGenerationAllowed:YES attachImagesAllowed:YES];
  [view_controller loadViewIfNeeded];

  UIButton* aim_button = FindViewById<UIButton>(view_controller.view,
                                                kNTPAIMQuickActionIdentifier);
  UIButton* image_gen_button = FindViewById<UIButton>(
      view_controller.view, kNTPAIMImageGenerationQuickActionIdentifier);
  UIButton* incognito_button = FindViewById<UIButton>(
      view_controller.view, kNTPIncognitoQuickActionIdentifier);
  ASSERT_NE(nil, aim_button);
  ASSERT_NE(nil, image_gen_button);
  ASSERT_NE(nil, incognito_button);

  EXPECT_FALSE(aim_button.hidden);
  EXPECT_FALSE(image_gen_button.hidden);
  EXPECT_TRUE(incognito_button.hidden);
}

// Tests that dynamically toggling `setIncognitoAllowed:` in Split Toolbar
// multi-chip mode updates visibility and stack distribution without Auto Layout
// conflicts.
TEST_F(NewTabPageQuickActionsViewControllerTest,
       UpdatesStackDistributionInSplitToolbarMultiChipArm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kAimButtonRefactor,
        {{kAimButtonRefactorArmParam,
          base::NumberToString(static_cast<int>(
              AimButtonRefactorArm::kImageGenerationQuickAction))}}},
       {kAIMNTPEntrypointTablet, {}}},
      /*disabled_features=*/{});

  NewTabPageQuickActionsViewController* view_controller =
      CreateAndConfigureViewController();
  view_controller.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  view_controller.traitOverrides.verticalSizeClass =
      UIUserInterfaceSizeClassRegular;
  [view_controller setAIMImageGenerationAllowed:YES attachImagesAllowed:YES];
  [view_controller loadViewIfNeeded];

  UIView* container = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 390, 100)];
  view_controller.view.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:view_controller.view];
  [NSLayoutConstraint activateConstraints:@[
    [view_controller.view.leadingAnchor
        constraintEqualToAnchor:container.leadingAnchor],
    [view_controller.view.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [view_controller.view.topAnchor
        constraintEqualToAnchor:container.topAnchor],
  ]];
  [container layoutIfNeeded];

  UIStackView* stack_view = FindViewByClass<UIStackView>(view_controller.view);
  UIButton* aim_button = FindViewById<UIButton>(view_controller.view,
                                                kNTPAIMQuickActionIdentifier);
  UIButton* image_gen_button = FindViewById<UIButton>(
      view_controller.view, kNTPAIMImageGenerationQuickActionIdentifier);
  UIButton* incognito_button = FindViewById<UIButton>(
      view_controller.view, kNTPIncognitoQuickActionIdentifier);
  ASSERT_NE(nil, stack_view);
  ASSERT_NE(nil, aim_button);
  ASSERT_NE(nil, image_gen_button);
  ASSERT_NE(nil, incognito_button);

  EXPECT_EQ(UIStackViewDistributionFill, stack_view.distribution);
  EXPECT_FALSE(incognito_button.hidden);

  [view_controller setIncognitoAllowed:NO];
  [container layoutIfNeeded];
  EXPECT_TRUE(incognito_button.hidden);
  EXPECT_FALSE(aim_button.hidden);
  EXPECT_FALSE(image_gen_button.hidden);
  EXPECT_EQ(UIStackViewDistributionFillEqually, stack_view.distribution);
  EXPECT_GT(aim_button.frame.size.width, 0.0);

  [view_controller setIncognitoAllowed:YES];
  [container layoutIfNeeded];
  EXPECT_FALSE(incognito_button.hidden);
  EXPECT_EQ(UIStackViewDistributionFill, stack_view.distribution);
  EXPECT_GT(incognito_button.frame.size.width, 0.0);
}

}  // namespace
