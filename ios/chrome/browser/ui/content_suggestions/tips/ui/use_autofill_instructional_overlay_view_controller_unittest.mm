// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/ui/use_autofill_instructional_overlay_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests for the `UseAutofillInstructionalOverlayViewControllerTest` class.
class UseAutofillInstructionalOverlayViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    view_controller_ =
        [[UseAutofillInstructionalOverlayViewController alloc] init];

    [view_controller_ view];
  }

 protected:
  UseAutofillInstructionalOverlayViewController* view_controller_ = nil;
};

// Tests that the strings and accessibility identifier are correctly set.
TEST_F(UseAutofillInstructionalOverlayViewControllerTest,
       ShouldSetCorrectStringsAndAccessibilityIdentifier) {
  EXPECT_EQ([view_controller_ steps].count, 3u);
  EXPECT_NSEQ([view_controller_ steps][0],
              l10n_util::GetNSString(
                  IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_1));
  EXPECT_NSEQ([view_controller_ steps][1],
              l10n_util::GetNSString(
                  IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_2));
  EXPECT_NSEQ([view_controller_ steps][2],
              l10n_util::GetNSString(
                  IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TUTORIAL_STEP_3));

  EXPECT_NSEQ([[view_controller_ view] accessibilityIdentifier],
              @"kUseAutofillInstructionalOverlayAXID");
}
