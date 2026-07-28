// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_style.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_keyboard_state_provider.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface SecondaryToolbarViewController (Testing)
@property(nonatomic) BOOL locationIndicatorActive;
- (void)collapsedToolbarButtonTapped;
@end

class SecondaryToolbarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    button_factory_ = [[LegacyToolbarButtonFactory alloc]
        initWithStyle:ToolbarStyle::kNormal];
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];

    view_controller_ = [[SecondaryToolbarViewController alloc] init];
    view_controller_.buttonFactory = button_factory_;
    view_controller_.layoutGuideCenter = layout_guide_center_;

    // Force view loading to initialize subviews and constraints.
    (void)view_controller_.view;
  }

  void TearDown() override {
    view_controller_ = nil;
    button_factory_ = nil;
    layout_guide_center_ = nil;
    PlatformTest::TearDown();
  }

  SecondaryToolbarViewController* view_controller_;
  LegacyToolbarButtonFactory* button_factory_;
  LayoutGuideCenter* layout_guide_center_;
};

// Tests that the controller initializes correctly.
TEST_F(SecondaryToolbarViewControllerTest, Initialization) {
  EXPECT_NE(view_controller_, nil);
  EXPECT_FALSE(view_controller_.hasOmnibox);
  EXPECT_FALSE(view_controller_.locationIndicatorActive);
}

// Tests that transitioning locationBarViewController to nil when
// locationIndicatorActive is YES correctly resets locationIndicatorActive to
// NO.
TEST_F(SecondaryToolbarViewControllerTest,
       ResetsLocationIndicatorActiveOnLocationBarRemoval) {
  UIViewController* location_bar = [[UIViewController alloc] init];
  view_controller_.locationBarViewController = location_bar;
  EXPECT_TRUE(view_controller_.hasOmnibox);

  view_controller_.locationIndicatorActive = YES;
  EXPECT_TRUE(view_controller_.locationIndicatorActive);

  // Transition locationBarViewController to nil.
  view_controller_.locationBarViewController = nil;

  // hasOmnibox should be NO, and locationIndicatorActive should have been reset
  // to NO.
  EXPECT_FALSE(view_controller_.hasOmnibox);
  EXPECT_FALSE(view_controller_.locationIndicatorActive);

  // Tapping the collapsed toolbar button should not crash now because
  // locationIndicatorActive is NO.
  [view_controller_ collapsedToolbarButtonTapped];
}

// Tests that setting locationIndicatorActive to NO does not exit forced
// fullscreen mode if the Find in Page navigator is visible.
TEST_F(SecondaryToolbarViewControllerTest,
       DoesNotExitFullscreenWhenFindNavigatorVisibleOnDeactivation) {
  base::test::ScopedFeatureList scoped_feature_list(kFullscreenRefactoring);

  id keyboard_provider =
      OCMProtocolMock(@protocol(SecondaryToolbarKeyboardStateProvider));
  view_controller_.keyboardStateProvider = keyboard_provider;

  id fullscreen_commands = OCMProtocolMock(@protocol(FullscreenCommands));
  view_controller_.fullscreenCommands = fullscreen_commands;

  // Activate location indicator.
  OCMExpect([fullscreen_commands
      enterFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                        animated:YES]);
  view_controller_.locationIndicatorActive = YES;
  EXPECT_TRUE(view_controller_.locationIndicatorActive);

  // When Find in Page navigator is visible, deactivating location indicator
  // should NOT call exitFullscreenWithTrigger.
  OCMStub([keyboard_provider isFindNavigatorVisibleForWebContent])
      .andReturn(YES);
  OCMReject([fullscreen_commands
      exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                       animated:YES]);

  view_controller_.locationIndicatorActive = NO;
  EXPECT_FALSE(view_controller_.locationIndicatorActive);

  EXPECT_OCMOCK_VERIFY(fullscreen_commands);
}

// Tests that setting locationIndicatorActive to NO exits forced fullscreen
// mode if the Find in Page navigator is not visible.
TEST_F(SecondaryToolbarViewControllerTest,
       ExitsFullscreenWhenFindNavigatorNotVisibleOnDeactivation) {
  base::test::ScopedFeatureList scoped_feature_list(kFullscreenRefactoring);

  id keyboard_provider =
      OCMProtocolMock(@protocol(SecondaryToolbarKeyboardStateProvider));
  view_controller_.keyboardStateProvider = keyboard_provider;

  id fullscreen_commands = OCMProtocolMock(@protocol(FullscreenCommands));
  view_controller_.fullscreenCommands = fullscreen_commands;

  // Activate location indicator.
  OCMExpect([fullscreen_commands
      enterFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                        animated:YES]);
  view_controller_.locationIndicatorActive = YES;
  EXPECT_TRUE(view_controller_.locationIndicatorActive);

  // When Find in Page navigator is not visible, deactivating location indicator
  // should call exitFullscreenWithTrigger.
  OCMStub([keyboard_provider isFindNavigatorVisibleForWebContent])
      .andReturn(NO);
  OCMExpect([fullscreen_commands
      exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                       animated:YES]);

  view_controller_.locationIndicatorActive = NO;
  EXPECT_FALSE(view_controller_.locationIndicatorActive);

  EXPECT_OCMOCK_VERIFY(fullscreen_commands);
}
