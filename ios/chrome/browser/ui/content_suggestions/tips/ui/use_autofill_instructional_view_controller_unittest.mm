// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/ui/use_autofill_instructional_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests for the `UseAutofillInstructionalViewControllerTest` class.
class UseAutofillInstructionalViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    view_controller_ = [[UseAutofillInstructionalViewController alloc] init];

    [view_controller_ view];
  }

 protected:
  UseAutofillInstructionalViewController* view_controller_ = nil;
};

// Tests that the strings and accessibility identifier are correctly set.
TEST_F(UseAutofillInstructionalViewControllerTest,
       StringsAndAccessibilityIdentifier) {
  EXPECT_NSEQ([view_controller_ animationName], @"use_autofill");
  EXPECT_NSEQ([view_controller_ animationNameDarkMode],
              @"use_autofill_darkmode");
  EXPECT_NSEQ([view_controller_ animationBackgroundColor],
              [UIColor colorNamed:kSecondaryBackgroundColor]);

  EXPECT_NSEQ(
      view_controller_.animationTextProvider[@"use_keyboard"],
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD));
  EXPECT_NSEQ(
      view_controller_.animationTextProvider[@"use_password"],
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD));

  EXPECT_NSEQ(
      [view_controller_ titleString],
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_TITLE));
  EXPECT_NSEQ([view_controller_ subtitleString],
              l10n_util::GetNSString(
                  IDS_IOS_MAGIC_STACK_TIP_AUTOFILL_PASSWORDS_DESCRIPTION));
  EXPECT_NSEQ([view_controller_ primaryActionString],
              l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_GOT_IT));
  EXPECT_NSEQ([view_controller_ secondaryActionString],
              l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_SHOW_ME_HOW));

  EXPECT_NSEQ([[view_controller_ view] accessibilityIdentifier],
              @"kUseAutofillInstructionalAXID");
}
