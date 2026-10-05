// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_position.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_test_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class ComposeboxInputPlateViewControllerTest : public PlatformTest {
 protected:
  ComposeboxInputPlateViewControllerTest() {
    ComposeboxTheme* theme = [[ComposeboxTheme alloc]
        initWithInputPlatePosition:ComposeboxInputPlatePosition::kBottom
                         incognito:NO
                             isNTP:NO];
    view_controller_ = [[ComposeboxInputPlateViewController alloc]
        initWithTheme:theme
           entrypoint:ComposeboxEntrypoint::kOther];
    // Like the mediator, set the state before the view loads, as the mode pills
    // get their titles from the UI config.
    ComposeboxUIInputState* state = [[ComposeboxUIInputState alloc] init];
    state.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
    [view_controller_ setUIInputState:state];
    [view_controller_ loadViewIfNeeded];
  }

  ComposeboxInputPlateViewController* view_controller_;
};

// Test that the Plus button shows its label in the Large Content Viewer, as it
// only has an image.
TEST_F(ComposeboxInputPlateViewControllerTest, TestPlusButtonLargeContent) {
  UIView* plus_button = FindViewWithIdentifier(
      view_controller_.view, kComposeboxPlusButtonAccessibilityIdentifier);
  ASSERT_TRUE(plus_button);

  EXPECT_TRUE(plus_button.showsLargeContentViewer);
  EXPECT_TRUE(HasLargeContentViewerInteraction(plus_button));
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL),
              plus_button.largeContentTitle);
}

// Test that the AI Mode pill shows the Large Content Viewer.
TEST_F(ComposeboxInputPlateViewControllerTest, TestAIMButtonLargeContent) {
  UIButton* aim_button = view_controller_.aimButton;
  ASSERT_TRUE(aim_button);

  EXPECT_TRUE(aim_button.showsLargeContentViewer);
  EXPECT_TRUE(HasLargeContentViewerInteraction(aim_button));
}

// Test that the Create Image pill shows the Large Content Viewer.
TEST_F(ComposeboxInputPlateViewControllerTest,
       TestImageGenerationButtonLargeContent) {
  UIButton* image_generation_button = view_controller_.imageGenerationButton;
  ASSERT_TRUE(image_generation_button);

  EXPECT_TRUE(image_generation_button.showsLargeContentViewer);
  EXPECT_TRUE(HasLargeContentViewerInteraction(image_generation_button));
}

}  // namespace
