// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_position.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr CGFloat kWindowWidth = 375.0f;
constexpr CGFloat kWindowHeight = 800.0f;
constexpr CGFloat kExpectedIpadWindowControlsOffset = 70.0f;
constexpr CGFloat kExpectedCloseButtonDefaultPadding = 10.0f;

class ComposeboxViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    theme_ = [[ComposeboxTheme alloc]
        initWithInputPlatePosition:ComposeboxInputPlatePosition::kTop
                         incognito:NO
                             isNTP:NO];
    view_controller_ = [[ComposeboxViewController alloc] initWithTheme:theme_];
    input_view_controller_ = [[ComposeboxInputPlateViewController alloc]
        initWithTheme:theme_
           entrypoint:ComposeboxEntrypoint::kOther];
    ComposeboxUIInputState* input_state = [[ComposeboxUIInputState alloc] init];
    input_state.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
    [input_view_controller_ setUIInputState:input_state];
  }

  void TearDown() override {
    input_view_controller_ = nil;
    view_controller_ = nil;
    theme_ = nil;
    PlatformTest::TearDown();
  }

  // Configures `view_controller_` with `idiom`, horizontal `size_class`, and
  // `layout_direction`, adds `input_view_controller_`, and triggers layout.
  void SetUpViewWithTraits(UIUserInterfaceIdiom idiom,
                           UIUserInterfaceSizeClass size_class,
                           UITraitEnvironmentLayoutDirection layout_direction) {
    view_controller_.traitOverrides.userInterfaceIdiom = idiom;
    view_controller_.traitOverrides.horizontalSizeClass = size_class;
    view_controller_.traitOverrides.verticalSizeClass =
        UIUserInterfaceSizeClassRegular;
    view_controller_.traitOverrides.layoutDirection = layout_direction;
    view_controller_.view.frame = CGRectMake(0, 0, kWindowWidth, kWindowHeight);
    view_controller_.view.semanticContentAttribute =
        layout_direction == UITraitEnvironmentLayoutDirectionRightToLeft
            ? UISemanticContentAttributeForceRightToLeft
            : UISemanticContentAttributeForceLeftToRight;

    [view_controller_ addInputViewController:input_view_controller_];
    [view_controller_.view setNeedsLayout];
    [view_controller_.view layoutIfNeeded];
  }

  ComposeboxTheme* theme_;
  ComposeboxViewController* view_controller_;
  ComposeboxInputPlateViewController* input_view_controller_;
};

// Test that on iPad in compact horizontal size class (LTR), the input plate
// leading edge is offset to avoid the top-left window controls (traffic
// lights).
TEST_F(ComposeboxViewControllerTest,
       TopPositionOnIPadCompactLTRAppliesWindowControlsOffset) {
  SetUpViewWithTraits(UIUserInterfaceIdiomPad, UIUserInterfaceSizeClassCompact,
                      UITraitEnvironmentLayoutDirectionLeftToRight);

  EXPECT_EQ(CGRectGetMinX(input_view_controller_.view.frame),
            kExpectedIpadWindowControlsOffset);
  EXPECT_EQ(kWindowWidth - CGRectGetMaxX(view_controller_.closeButton.frame),
            kExpectedCloseButtonDefaultPadding);
}

// Test that on iPad in compact horizontal size class (RTL), the close button
// trailing edge (top-left corner in RTL) is offset to avoid the window
// controls, while the input plate leading edge (top-right in RTL) uses the
// standard margin.
TEST_F(ComposeboxViewControllerTest,
       TopPositionOnIPadCompactRTLAppliesWindowControlsOffset) {
  SetUpViewWithTraits(UIUserInterfaceIdiomPad, UIUserInterfaceSizeClassCompact,
                      UITraitEnvironmentLayoutDirectionRightToLeft);

  EXPECT_EQ(CGRectGetMinX(view_controller_.closeButton.frame),
            kExpectedIpadWindowControlsOffset);
  EXPECT_EQ(kWindowWidth - CGRectGetMaxX(input_view_controller_.view.frame),
            kInputPlateMargin);
}

// Test that on iPhone in compact horizontal size class, the standard input
// plate margin is used without the iPad window controls offset.
TEST_F(ComposeboxViewControllerTest,
       TopPositionOnIPhoneCompactUsesDefaultMargin) {
  SetUpViewWithTraits(UIUserInterfaceIdiomPhone,
                      UIUserInterfaceSizeClassCompact,
                      UITraitEnvironmentLayoutDirectionLeftToRight);

  EXPECT_EQ(CGRectGetMinX(input_view_controller_.view.frame),
            kInputPlateMargin);
  EXPECT_EQ(kWindowWidth - CGRectGetMaxX(view_controller_.closeButton.frame),
            kExpectedCloseButtonDefaultPadding);
}

}  // namespace
