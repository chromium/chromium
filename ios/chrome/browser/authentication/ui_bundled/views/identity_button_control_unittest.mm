// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/views/identity_button_control.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using IdentityButtonControlTest = PlatformTest;

// Tests that the default state has `interactive` set to `YES`, interaction
// enabled, and button accessibility trait.
TEST_F(IdentityButtonControlTest, DefaultState) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  EXPECT_TRUE(control.interactive);
  EXPECT_EQ(control.arrowDirection, IdentityButtonControlArrowDown);
  EXPECT_TRUE(control.userInteractionEnabled);
  EXPECT_TRUE(control.accessibilityTraits & UIAccessibilityTraitButton);
}

// Tests that setting `interactive` to `NO` before configuring an unmanaged
// identity with name and email disables user interaction, removes the button
// accessibility trait, sets the name and email accessibility label, and leaves
// no ambiguous layout.
TEST_F(IdentityButtonControlTest, NonInteractiveWithNameAndEmail) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectMake(0, 0, 320, 60)];
  control.interactive = NO;
  [control setIdentityName:@"Jane Doe" email:@"jane@example.com" managed:NO];
  [control layoutIfNeeded];

  EXPECT_FALSE(control.interactive);
  EXPECT_FALSE(control.userInteractionEnabled);
  EXPECT_EQ(control.accessibilityTraits, UIAccessibilityTraitNone);
  EXPECT_NSEQ(
      control.accessibilityLabel,
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME,
                              base::SysNSStringToUTF16(@"Jane Doe"),
                              base::SysNSStringToUTF16(@"jane@example.com")));
  EXPECT_FALSE(control.hasAmbiguousLayout);
  for (UIView* subview in control.subviews) {
    EXPECT_FALSE(subview.hasAmbiguousLayout);
  }
}

// Tests that setting `interactive` to `NO` before configuring an unmanaged
// identity with only an email sets the email as the accessibility label.
TEST_F(IdentityButtonControlTest, NonInteractiveEmailOnly) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  control.interactive = NO;
  [control setIdentityName:nil email:@"jane@example.com" managed:NO];

  EXPECT_EQ(control.accessibilityTraits, UIAccessibilityTraitNone);
  EXPECT_NSEQ(control.accessibilityLabel, @"jane@example.com");
}

// Tests that setting `interactive` to `NO` before configuring a managed
// identity with name and email includes the managed organization description.
TEST_F(IdentityButtonControlTest, NonInteractiveManagedWithNameAndEmail) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  control.interactive = NO;
  [control setIdentityName:@"Jane Doe" email:@"jane@example.com" managed:YES];

  EXPECT_EQ(control.accessibilityTraits, UIAccessibilityTraitNone);
  EXPECT_NSEQ(
      control.accessibilityLabel,
      l10n_util::GetNSStringF(
          IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED,
          base::SysNSStringToUTF16(@"Jane Doe"),
          base::SysNSStringToUTF16(@"jane@example.com")));
}

// Tests that setting `interactive` to `NO` before configuring a managed
// identity with only an email includes the managed organization description.
TEST_F(IdentityButtonControlTest, NonInteractiveManagedEmailOnly) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  control.interactive = NO;
  [control setIdentityName:nil email:@"jane@example.com" managed:YES];

  EXPECT_EQ(control.accessibilityTraits, UIAccessibilityTraitNone);
  EXPECT_NSEQ(
      control.accessibilityLabel,
      l10n_util::GetNSStringF(
          IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_EMAIL_MANAGED,
          base::SysNSStringToUTF16(@"jane@example.com")));
}

// Tests that updating `interactive` after `-setIdentityName:email:managed:`
// refreshes accessibility traits and label accordingly.
TEST_F(IdentityButtonControlTest, InteractiveUpdated) {
  IdentityButtonControl* control =
      [[IdentityButtonControl alloc] initWithFrame:CGRectMake(0, 0, 320, 60)];
  [control setIdentityName:@"Jane Doe" email:@"jane@example.com" managed:NO];
  EXPECT_TRUE(control.accessibilityTraits & UIAccessibilityTraitButton);

  control.interactive = NO;
  EXPECT_FALSE(control.userInteractionEnabled);
  EXPECT_EQ(control.accessibilityTraits, UIAccessibilityTraitNone);
  EXPECT_NSEQ(
      control.accessibilityLabel,
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_VIEW_ACCESSIBILITY_LABEL_NAME,
                              base::SysNSStringToUTF16(@"Jane Doe"),
                              base::SysNSStringToUTF16(@"jane@example.com")));

  control.interactive = YES;
  [control layoutIfNeeded];
  EXPECT_TRUE(control.userInteractionEnabled);
  EXPECT_TRUE(control.accessibilityTraits & UIAccessibilityTraitButton);
  EXPECT_NSEQ(control.accessibilityLabel,
              l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_NAME_AND_EMAIL,
                  base::SysNSStringToUTF16(@"Jane Doe"),
                  base::SysNSStringToUTF16(@"jane@example.com")));
  EXPECT_FALSE(control.hasAmbiguousLayout);
  for (UIView* subview in control.subviews) {
    EXPECT_FALSE(subview.hasAmbiguousLayout);
  }
}

}  // namespace
