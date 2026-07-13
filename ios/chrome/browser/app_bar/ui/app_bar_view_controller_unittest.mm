// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_background_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AppBarViewController (Testing) <UIContextMenuInteractionDelegate>
- (void)setButtonsTitleAlpha:(CGFloat)buttonsTitleAlpha
           animationDuration:(NSTimeInterval)duration;
@end

// A test implementation of UIContextMenuInteractionAnimating to simulate
// dismissal animations.
@interface TestContextMenuInteractionAnimating
    : NSObject <UIContextMenuInteractionAnimating>
@property(nonatomic, copy) void (^animations)(void);
@property(nonatomic, copy) void (^completion)(void);
@end

@implementation TestContextMenuInteractionAnimating
- (void)addAnimations:(void (^)(void))animations {
  self.animations = animations;
}
- (void)addCompletion:(void (^)(void))completion {
  self.completion = completion;
}
- (UIViewController*)previewViewController {
  return nil;
}
@end

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

// Tests for the AppBarViewController state.
class AppBarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    layout_state_ = [[LayoutState alloc] init];
    [layout_state_
        setAppBarPosition:AppBarPosition::kBottom
                  passKey:LayoutStateTestPassKeyFactory::CreateSceneKey()];
    view_controller_ = [[AppBarViewController alloc] init];
    view_controller_.layoutState = layout_state_;
    [view_controller_ view];
  }

  void TearDown() override {
    view_controller_ = nil;
    layout_state_ = nil;
    PlatformTest::TearDown();
  }

  AppBarViewController* view_controller_;
  LayoutState* layout_state_;

  // Helper to access the private `_openNewTabButton` ivar using KVC.
  UIButton* openNewTabButton() {
    return [view_controller_ valueForKey:@"openNewTabButton"];
  }

  // Helper to access the private `_tabGridButton` ivar using KVC.
  UIButton* tabGridButton() {
    return [view_controller_ valueForKey:@"tabGridButton"];
  }

  // Helper to access the private `_assistantButton` ivar using KVC.
  UIButton* assistantButton() {
    return [view_controller_ valueForKey:@"assistantButton"];
  }

  // Helper to access the private `_assistantHighlightView` ivar using KVC.
  UIView* assistantHighlightView() {
    return [view_controller_ valueForKey:@"assistantHighlightView"];
  }
};

// Tests that the new tab button shows the menu as primary action when the
// tab groups page is visible in the tab grid.
TEST_F(AppBarViewControllerTest, TestShowsMenuAsPrimaryActionForTabGroupsPage) {
  [view_controller_ setTabGroupsPageVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);

  [view_controller_ setTabGroupsPageVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that the new tab button shows the menu as primary action when a
// tab group is visible in the tab grid.
TEST_F(AppBarViewControllerTest,
       TestShowsMenuAsPrimaryActionForTabGroupVisible) {
  [view_controller_ setTabGridVisible:YES];
  [view_controller_ setTabGroupVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);

  [view_controller_ setTabGroupVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that the new tab button does NOT show the menu as primary action when a
// tab group is visible but we are not in the tab grid (e.g. browsing).
TEST_F(AppBarViewControllerTest,
       TestShowsMenuAsPrimaryActionForTabGroupVisibleButGridHidden) {
  [view_controller_ setTabGridVisible:NO];
  [view_controller_ setTabGroupVisible:YES];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that rotation toggles stack view distribution, width constraints, and
// spacers.
TEST_F(AppBarViewControllerTest,
       TestRotationTogglesDistributionConstraintsAndSpacers) {
  [view_controller_ updateForAngle:0];

  UIStackView* stackView = [view_controller_ valueForKey:@"stackView"];
  NSArray<NSLayoutConstraint*>* buttonWidthConstraints =
      [view_controller_ valueForKey:@"buttonWidthConstraints"];
  UIView* spacer1 = [view_controller_ valueForKey:@"_leadingSpacer"];
  UIView* spacer2 = [view_controller_ valueForKey:@"_trailingSpacer"];

  EXPECT_EQ(stackView.distribution, UIStackViewDistributionFillEqually);
  for (NSLayoutConstraint* constraint in buttonWidthConstraints) {
    EXPECT_FALSE(constraint.active);
  }
  EXPECT_TRUE(spacer1.hidden);
  EXPECT_TRUE(spacer2.hidden);

  [view_controller_ updateForAngle:M_PI_2];

  EXPECT_EQ(stackView.distribution, UIStackViewDistributionEqualSpacing);
  for (NSLayoutConstraint* constraint in buttonWidthConstraints) {
    EXPECT_TRUE(constraint.active);
  }
  EXPECT_FALSE(spacer1.hidden);
  EXPECT_FALSE(spacer2.hidden);
}

// Tests that rotation updates the stack view bottom constraint.
TEST_F(AppBarViewControllerTest, TestRotationUpdatesStackViewConstraints) {
  [view_controller_ updateForAngle:0];

  NSLayoutConstraint* bottomConstraint =
      [view_controller_ valueForKey:@"stackViewBottomConstraint"];

  EXPECT_EQ(bottomConstraint.constant, 0.0);

  [view_controller_ updateForAngle:M_PI_2];

  EXPECT_EQ(bottomConstraint.constant, 0.0);
}

// Tests that rotation updates the height constraint dynamically.
TEST_F(AppBarViewControllerTest, TestRotationUpdatesHeightConstraint) {
  [view_controller_ updateForAngle:0];

  NSLayoutConstraint* heightConstraint =
      [view_controller_ valueForKey:@"heightConstraint"];

  EXPECT_EQ(heightConstraint.constant, AppBarHeightPortrait());

  [view_controller_ updateForAngle:M_PI_2];

  EXPECT_EQ(heightConstraint.constant, AppBarHeightLandscape());
}

// Tests that the tab grid button's image color transformer always returns clear
// color.
TEST_F(AppBarViewControllerTest, TestTabGridButtonImageIsClear) {
  UIButton* button = tabGridButton();
  ASSERT_NE(button, nil);

  // Test normal state.
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIButtonConfiguration* config = button.configuration;
  ASSERT_NE(config, nil);
  ASSERT_NE(config.imageColorTransformer, nil);
  EXPECT_EQ(config.imageColorTransformer(UIColor.whiteColor),
            UIColor.clearColor);

  // Test highlighted state.
  button.highlighted = YES;
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  config = button.configuration;
  ASSERT_NE(config, nil);
  ASSERT_NE(config.imageColorTransformer, nil);
  EXPECT_EQ(config.imageColorTransformer(UIColor.whiteColor),
            UIColor.clearColor);

  // Test disabled state.
  button.highlighted = NO;
  button.enabled = NO;
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  config = button.configuration;
  ASSERT_NE(config, nil);
  ASSERT_NE(config.imageColorTransformer, nil);
  EXPECT_EQ(config.imageColorTransformer(UIColor.whiteColor),
            UIColor.clearColor);
}

// Tests that the menu property on the new tab button is managed correctly.
TEST_F(AppBarViewControllerTest, TestNewTabButtonMenuManagement) {
  UIMenu* dummyMenu = [UIMenu menuWithTitle:@"Test" children:@[]];

  // Set the menu.
  [view_controller_ setMenu:dummyMenu forButtonType:AppBarButtonTypeNewTab];

  // By default, `showsMenuAsPrimaryAction` is NO, so the button's menu should
  // be nil.
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
  EXPECT_NSEQ(openNewTabButton().menu, nil);

  // When tab groups page is visible, `showsMenuAsPrimaryAction` is YES, so the
  // button's menu should be set.
  [view_controller_ setTabGroupsPageVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);
  EXPECT_NSEQ(openNewTabButton().menu, dummyMenu);

  // When tab groups page is hidden again, `showsMenuAsPrimaryAction` is NO, so
  // the button's menu should be nil.
  [view_controller_ setTabGroupsPageVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
  EXPECT_NSEQ(openNewTabButton().menu, nil);

  // When tab grid and tab group are visible, `showsMenuAsPrimaryAction` is YES,
  // so the button's menu should be set.
  [view_controller_ setTabGridVisible:YES];
  [view_controller_ setTabGroupVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);
  EXPECT_NSEQ(openNewTabButton().menu, dummyMenu);

  // When tab group is hidden, `showsMenuAsPrimaryAction` is NO, so the
  // button's menu should be nil.
  [view_controller_ setTabGroupVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
  EXPECT_NSEQ(openNewTabButton().menu, nil);
}

// Tests that the assistant button highlight state toggles the custom highlight
// view and does not set the button background color.
TEST_F(AppBarViewControllerTest, TestAssistantButtonHighlightState) {
  UIButton* button = assistantButton();
  ASSERT_NE(button, nil);

  // Initially not highlighted.
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIView* highlightView = assistantHighlightView();
  // It might be nil if not created yet.
  if (highlightView) {
    EXPECT_EQ(highlightView.alpha, 0.0);
  }
  EXPECT_FALSE(button.accessibilityTraits & UIAccessibilityTraitSelected);

  // Highlighted.
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:YES
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  highlightView = assistantHighlightView();
  ASSERT_NE(highlightView, nil);
  EXPECT_EQ(highlightView.alpha, 1.0);
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitSelected);

  // Verify button background color is clearColor, and highlightView is a
  // subview.
  UIButtonConfiguration* config = button.configuration;
  EXPECT_TRUE(config.background.backgroundColor == [UIColor clearColor]);
  EXPECT_EQ(highlightView.superview, button);

  // Not highlighted again.
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  EXPECT_EQ(highlightView.alpha, 0.0);
  EXPECT_FALSE(button.accessibilityTraits & UIAccessibilityTraitSelected);
}

// Tests that long-pressing a button temporarily unhides its title text when
// the global title alpha is 0 (fullscreen/shrunk state), and fades it back out
// upon dismissal.
TEST_F(AppBarViewControllerTest, TestTitleVisibilityDuringContextMenu) {
  // Set titles hidden (simulating fullscreen/shrunk state).
  [view_controller_ setButtonsTitleAlpha:0.0 animationDuration:0];

  UIButton* button = tabGridButton();
  ASSERT_NE(button, nil);

  // Verify title is hidden initially (alpha = 0).
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];
  EXPECT_EQ(button.titleLabel.alpha, 0.0);

  // Simulate long-press gesture triggering context menu configuration.
  UIMenu* dummyMenu = [UIMenu menuWithTitle:@"Test" children:@[]];
  [view_controller_ setMenu:dummyMenu forButtonType:AppBarButtonTypeTabGrid];

  UIContextMenuInteraction* interaction =
      [[UIContextMenuInteraction alloc] initWithDelegate:view_controller_];
  [button addInteraction:interaction];

  UIContextMenuConfiguration* menuConfig =
      [view_controller_ contextMenuInteraction:interaction
                configurationForMenuAtLocation:CGPointZero];
  EXPECT_NE(menuConfig, nil);

  // Verify that the button currently being previewed is set, and its title is
  // now visible (alpha = 1).
  UIButton* previewedButton = [view_controller_ valueForKey:@"previewedButton"];
  EXPECT_EQ(previewedButton, button);

  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];
  EXPECT_EQ(button.titleLabel.alpha, 1.0);

  // Simulate dismissal.
  TestContextMenuInteractionAnimating* animator =
      [[TestContextMenuInteractionAnimating alloc] init];
  [view_controller_ contextMenuInteraction:interaction
                   willEndForConfiguration:menuConfig
                                  animator:animator];

  // Execute the animation block.
  ASSERT_NE(animator.animations, nil);
  animator.animations();

  // Verify that previewedButton is cleared, and title is hidden again (alpha =
  // 0).
  previewedButton = [view_controller_ valueForKey:@"previewedButton"];
  EXPECT_EQ(previewedButton, nil);

  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];
  EXPECT_EQ(button.titleLabel.alpha, 0.0);
}

// Tests that the assistant button in kAccount state sets the correct image and
// no title.
TEST_F(AppBarViewControllerTest, TestAssistantButtonStateAccount) {
  UIButton* button = assistantButton();
  ASSERT_NE(button, nil);

  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAccount
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIButtonConfiguration* config = button.configuration;
  if (IsAppBarLabelsHidden()) {
    EXPECT_EQ(config.title, nil);
  } else {
    EXPECT_NSEQ(config.title, l10n_util::GetNSString(IDS_IOS_APP_BAR_SIGN_IN));
  }
  EXPECT_NE(config.image, nil);
}

// Tests that the assistant button in kAccount state sets the avatar image if
// available.
TEST_F(AppBarViewControllerTest, TestAssistantButtonStateAccountWithAvatar) {
  UIButton* button = assistantButton();
  ASSERT_NE(button, nil);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(10, 10)];
  UIImage* mock_avatar =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context){
          // Draw nothing, just want an empty image with size 10x10.
      }];

  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAccount
                                highlighted:NO
                                    enabled:YES
                                     avatar:mock_avatar
                                   signedIn:YES];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIButtonConfiguration* config = button.configuration;
  if (IsAppBarLabelsHidden()) {
    EXPECT_EQ(config.title, nil);
  } else {
    EXPECT_NSEQ(config.title, l10n_util::GetNSString(IDS_IOS_APP_BAR_ACCOUNT));
  }
  ASSERT_NE(config.image, nil);
  EXPECT_EQ(config.image.size.width, 23);
  EXPECT_EQ(config.image.size.height, 23);
}

// Tests that the tab grid button has the correct accessibility label and
// selected traits based on tab grid visibility and tab group state.
TEST_F(AppBarViewControllerTest, TestTabGridButtonAccessibilityAndTraits) {
  UIButton* button = tabGridButton();
  ASSERT_NE(button, nil);

  // 1. By default, tab grid is not visible, and we are not in a tab group.
  [view_controller_ setTabGridVisible:NO];
  [view_controller_ setInTabGroup:NO];
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_APP_BAR_ALL_TABS));
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitButton);
  EXPECT_FALSE(button.accessibilityTraits & UIAccessibilityTraitSelected);

  // 2. Set tab grid visible.
  [view_controller_ setTabGridVisible:YES];
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_APP_BAR_ALL_TABS));
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitButton);
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitSelected);

  // 3. Set tab grid not visible, and enter tab group.
  [view_controller_ setTabGridVisible:NO];
  [view_controller_ setInTabGroup:YES];
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GROUP));
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitButton);
  EXPECT_FALSE(button.accessibilityTraits & UIAccessibilityTraitSelected);

  // 4. Set tab grid visible while in a tab group.
  [view_controller_ setTabGridVisible:YES];
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GROUP));
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitButton);
  EXPECT_TRUE(button.accessibilityTraits & UIAccessibilityTraitSelected);
}

// Tests that the tab grid button has the correct accessibility value.
TEST_F(AppBarViewControllerTest, TestTabGridButtonAccessibilityValue) {
  UIButton* button = tabGridButton();
  ASSERT_NE(button, nil);

  [view_controller_ updateTabCount:3];
  EXPECT_NSEQ(button.accessibilityValue, @"3");

  [view_controller_ updateTabCount:0];
  EXPECT_NSEQ(button.accessibilityValue, @"0");
}

// Tests that assistant button in kLens state sets correct image and title.
TEST_F(AppBarViewControllerTest, TestAssistantButtonStateLens) {
  UIButton* button = assistantButton();
  ASSERT_NE(button, nil);

  // Test full title when width is default (or 0).
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIButtonConfiguration* config = button.configuration;
  if (IsAppBarLabelsHidden()) {
    EXPECT_EQ(config.title, nil);
  } else {
    EXPECT_NSEQ(config.title,
                l10n_util::GetNSString(IDS_IOS_LENS_PRODUCT_NAME));
  }
  EXPECT_NE(config.image, nil);

  // Set the view width to a very small size to force truncation.
  view_controller_.view.frame = CGRectMake(0, 0, 100, 50);
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kLens
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  config = button.configuration;
  if (IsAppBarLabelsHidden()) {
    EXPECT_EQ(config.title, nil);
  } else {
    EXPECT_NSEQ(config.title,
                l10n_util::GetNSString(IDS_IOS_LENS_PRODUCT_NAME_TRUNCATED));
  }
}

// Tests that assistant button has correct accessibility label in portrait and
// rotated modes.
TEST_F(AppBarViewControllerTest, TestAssistantButtonAccessibilityLabel) {
  UIButton* button = assistantButton();
  ASSERT_NE(button, nil);

  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];

  // Verify in portrait mode (angle 0).
  [view_controller_ updateForAngle:0];
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_APP_BAR_ASK_GEMINI));

  // Verify in rotated/landscape mode (angle M_PI_2). Title should be nil, but
  // accessibilityLabel should still be set.
  [view_controller_ updateForAngle:M_PI_2];
  EXPECT_EQ(button.configuration.title, nil);
  EXPECT_NSEQ(button.accessibilityLabel,
              l10n_util::GetNSString(IDS_IOS_APP_BAR_ASK_GEMINI));
}

// Tests that when kAppBarHideLabels is enabled, viewWillLayoutSubviews does not
// cause infinite re-entrancy or crashes due to title updaters repeatedly
// modifying button configurations.
TEST_F(AppBarViewControllerTest, TestIdempotentTitleUpdatesWithHiddenLabels) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAppBarHideLabels);

  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:NO
                                    enabled:YES
                                     avatar:nil
                                   signedIn:NO];
  // Trigger multiple layout passes to verify idempotency and absence of
  // infinite recursion.
  [view_controller_.view setNeedsLayout];
  [view_controller_.view layoutIfNeeded];

  UIButton* assistantButton = [view_controller_ valueForKey:@"assistantButton"];
  EXPECT_EQ(assistantButton.configuration.title, nil);

  [view_controller_.view setNeedsLayout];
  [view_controller_.view layoutIfNeeded];

  EXPECT_EQ(assistantButton.configuration.title, nil);
}

using AppBarViewControllerTestManual = PlatformTest;

// Tests that setting incognito before the view is loaded correctly applies
// when the view is loaded.
TEST_F(AppBarViewControllerTestManual, TestIncognitoInitially) {
  AppBarViewController* vc = [[AppBarViewController alloc] init];
  [vc setIncognito:YES];

  // Trigger view load.
  UIView* view = vc.view;
  ASSERT_NE(view, nil);

  AppBarBackgroundView* backgroundView = [vc valueForKey:@"backgroundView"];
  EXPECT_TRUE(backgroundView.incognito);

  UIButton* assistantButton = [vc valueForKey:@"assistantButton"];
  EXPECT_FALSE(assistantButton.enabled);
  EXPECT_TRUE(assistantButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

// Tests that the open new tab button only logs shortcut user action metrics
// when the tab grid is not visible, and logs the NTP variant when the NTP is
// visible.
TEST_F(AppBarViewControllerTest, TestNewTabButtonMetrics) {
  base::UserActionTester user_action_tester;

  // Set tab grid not visible (browsing mode) and NTP visible.
  [view_controller_ setTabGridVisible:NO];
  [view_controller_ setNTPVisible:YES isStartSurface:NO];

  // Trigger tap.
  UIButton* button = openNewTabButton();
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_EQ(user_action_tester.GetActionCount("MobileToolbarNewTabShortcut"),
            1);
  EXPECT_EQ(
      user_action_tester.GetActionCount("MobileToolbarNewTabShortcutOnNTP"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("MobileTabNewTab"), 1);

  // Set tab grid visible.
  [view_controller_ setTabGridVisible:YES];

  // Trigger tap again.
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];

  // The metrics should NOT increment because tab grid is visible.
  EXPECT_EQ(user_action_tester.GetActionCount("MobileToolbarNewTabShortcut"),
            1);
  EXPECT_EQ(
      user_action_tester.GetActionCount("MobileToolbarNewTabShortcutOnNTP"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("MobileTabNewTab"), 1);
}

}  // namespace
