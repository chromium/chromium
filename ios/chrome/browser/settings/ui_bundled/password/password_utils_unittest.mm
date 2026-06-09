// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_utils.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace password_manager {

using PasswordUtilsTest = PlatformTest;

// Tests CreateSetUpScreenLockAlert with default message and handlers.
TEST_F(PasswordUtilsTest, CreateSetUpScreenLockAlertDefault) {
  __block bool learn_how_tapped = false;

  UIAlertController* alert = CreateSetUpScreenLockAlert(nil, ^{
    learn_how_tapped = true;
  });

  ASSERT_NE(alert, nil);
  EXPECT_EQ(alert.preferredStyle, UIAlertControllerStyleAlert);

  EXPECT_NSEQ(alert.title,
              l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE));
  EXPECT_NSEQ(alert.message, l10n_util::GetNSString(
                                 IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT));

  ASSERT_EQ(alert.actions.count, 2u);

  // First action: Learn how
  EXPECT_NSEQ(
      alert.actions[0].title,
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW));
  EXPECT_EQ(alert.actions[0].style, UIAlertActionStyleDefault);
  void (^handler_learn)(UIAlertAction*) =
      [alert.actions[0] valueForKey:@"handler"];
  ASSERT_NE(handler_learn, nil);
  handler_learn(alert.actions[0]);
  EXPECT_TRUE(learn_how_tapped);

  // Second action: OK
  EXPECT_NSEQ(alert.actions[1].title, l10n_util::GetNSString(IDS_OK));
  EXPECT_EQ(alert.actions[1].style, UIAlertActionStyleDefault);

  EXPECT_EQ(alert.preferredAction, alert.actions[1]);
}

// Tests CreateSetUpScreenLockAlert with custom message and nil learn how
// handler.
TEST_F(PasswordUtilsTest, CreateSetUpScreenLockAlertCustomMessageNoLearnHow) {
  NSString* custom_message = @"Custom screenlock requirement";
  UIAlertController* alert = CreateSetUpScreenLockAlert(custom_message, nil);

  ASSERT_NE(alert, nil);
  EXPECT_NSEQ(alert.title,
              l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE));
  EXPECT_NSEQ(alert.message, custom_message);

  ASSERT_EQ(alert.actions.count, 1u);
  EXPECT_NSEQ(alert.actions[0].title, l10n_util::GetNSString(IDS_OK));
}

}  // namespace password_manager
