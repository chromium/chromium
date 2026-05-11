// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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

// Tests for the AppBarViewController state.
class AppBarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[AppBarViewController alloc] init];
    [view_controller_ view];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  AppBarViewController* view_controller_;

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
                                    enabled:YES];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  UIView* highlightView = assistantHighlightView();
  // It might be nil if not created yet, or created and hidden.
  if (highlightView) {
    EXPECT_TRUE(highlightView.hidden);
  }

  // Highlighted.
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:YES
                                    enabled:YES];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  highlightView = assistantHighlightView();
  ASSERT_NE(highlightView, nil);
  EXPECT_FALSE(highlightView.hidden);

  // Verify button background color is clearColor (we use customView instead).
  UIButtonConfiguration* config = button.configuration;
  EXPECT_TRUE(config.background.backgroundColor == [UIColor clearColor]);
  EXPECT_TRUE(config.background.customView != nil);

  // Not highlighted again.
  [view_controller_ setAssistantButtonState:AppBarAssistantButtonState::kAsk
                                highlighted:NO
                                    enabled:YES];
  [button setNeedsUpdateConfiguration];
  [button layoutIfNeeded];

  EXPECT_TRUE(highlightView.hidden);
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
  UIButtonConfiguration* config = button.configuration;
  __block CGFloat titleAlpha = -1.0;
  if (config.titleTextAttributesTransformer) {
    NSDictionary* attrs = config.titleTextAttributesTransformer(@{});
    UIColor* color = attrs[NSForegroundColorAttributeName];
    [color getRed:nil green:nil blue:nil alpha:&titleAlpha];
  }
  EXPECT_EQ(titleAlpha, 0.0);

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
  config = button.configuration;
  titleAlpha = -1.0;
  if (config.titleTextAttributesTransformer) {
    NSDictionary* attrs = config.titleTextAttributesTransformer(@{});
    UIColor* color = attrs[NSForegroundColorAttributeName];
    [color getRed:nil green:nil blue:nil alpha:&titleAlpha];
  }
  EXPECT_EQ(titleAlpha, 1.0);

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
  config = button.configuration;
  titleAlpha = -1.0;
  if (config.titleTextAttributesTransformer) {
    NSDictionary* attrs = config.titleTextAttributesTransformer(@{});
    UIColor* color = attrs[NSForegroundColorAttributeName];
    [color getRed:nil green:nil blue:nil alpha:&titleAlpha];
  }
  EXPECT_EQ(titleAlpha, 0.0);
}

}  // namespace
