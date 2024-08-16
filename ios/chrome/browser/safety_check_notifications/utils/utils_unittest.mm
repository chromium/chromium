// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

class SafetyCheckNotificationUtilsTest : public PlatformTest {};

#pragma mark - Test cases

// Tests if a notification is correctly generated for the compromised password
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsCompromisedPasswordNotificationForCompromisedState) {
  UNNotificationContent* compromised_password_notification =
      NotificationForPasswordCheckState(
          PasswordSafetyCheckState::kUnmutedCompromisedPasswords,
          {/* compromised */
           3,
           /* dismissed */
           1,
           /* reused */
           2,
           /* weak */
           3});

  EXPECT_NE(compromised_password_notification, nil);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);

  EXPECT_TRUE(
      [expected_title isEqualToString:compromised_password_notification.title]);

  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_COMPROMISED_PASSWORD, 3);

  EXPECT_TRUE(
      [expected_body isEqualToString:compromised_password_notification.body]);
}

// Tests if a notification is correctly generated for the weak password state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsWeakPasswordNotificationForWeakState) {
  UNNotificationContent* weak_password_notification =
      NotificationForPasswordCheckState(
          PasswordSafetyCheckState::kWeakPasswords, {/* compromised */
                                                     1,
                                                     /* dismissed */
                                                     2,
                                                     /* reused */
                                                     3,
                                                     /* weak */
                                                     4});

  EXPECT_NE(weak_password_notification, nil);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);

  EXPECT_TRUE(
      [expected_title isEqualToString:weak_password_notification.title]);

  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_WEAK_PASSWORD, 4);

  EXPECT_TRUE([expected_body isEqualToString:weak_password_notification.body]);
}

// Tests if a notification is correctly generated for the reused password state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsReusedPasswordNotificationForReusedState) {
  UNNotificationContent* reused_password_notification =
      NotificationForPasswordCheckState(
          PasswordSafetyCheckState::kReusedPasswords, {/* compromised */
                                                       2,
                                                       /* dismissed */
                                                       3,
                                                       /* reused */
                                                       5,
                                                       /* weak */
                                                       4});

  EXPECT_NE(reused_password_notification, nil);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);

  EXPECT_TRUE(
      [expected_title isEqualToString:reused_password_notification.title]);

  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_REUSED_PASSWORD, 5);

  EXPECT_TRUE(
      [expected_body isEqualToString:reused_password_notification.body]);
}

// Tests that no notification is generated for the safe password state.
TEST_F(SafetyCheckNotificationUtilsTest, ReturnsNothingForSafePasswordState) {
  UNNotificationContent* password_notification =
      NotificationForPasswordCheckState(PasswordSafetyCheckState::kSafe,
                                        {/* compromised */
                                         0,
                                         /* dismissed */
                                         0,
                                         /* reused */
                                         0,
                                         /* weak */
                                         0});

  EXPECT_EQ(password_notification, nil);
}

// Tests if a notification is correctly generated for the out-of-date Chrome
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsUpdateChromeNotificationForOutOfDateState) {
  UNNotificationContent* update_chrome_notification =
      NotificationForUpdateChromeCheckState(
          UpdateChromeSafetyCheckState::kOutOfDate);

  EXPECT_NE(update_chrome_notification, nil);

  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_UPDATE_BROWSER);

  EXPECT_TRUE(
      [expected_title isEqualToString:update_chrome_notification.title]);

  NSString* expected_body =
      l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);

  EXPECT_TRUE([expected_body isEqualToString:update_chrome_notification.body]);
}

// Tests that no notification is generated for the up-to-date Chrome state.
TEST_F(SafetyCheckNotificationUtilsTest, ReturnsNothingForUpToDateState) {
  UNNotificationContent* update_chrome_notification =
      NotificationForUpdateChromeCheckState(
          UpdateChromeSafetyCheckState::kUpToDate);

  EXPECT_EQ(update_chrome_notification, nil);
}

// Tests if a notification is correctly generated for the unsafe Safe Browsing
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsSafeBrowsingNotificationForUnsafeState) {
  UNNotificationContent* safe_browsing_notification =
      NotificationForSafeBrowsingCheckState(
          SafeBrowsingSafetyCheckState::kUnsafe);

  EXPECT_NE(safe_browsing_notification, nil);

  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_ADD_BROWSING_PROTECTION);

  EXPECT_TRUE(
      [expected_title isEqualToString:safe_browsing_notification.title]);

  NSString* expected_body = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_BROWSING_PROTECTION_OFF);

  EXPECT_TRUE([expected_body isEqualToString:safe_browsing_notification.body]);
}

// Tests that no notification is generated for the safe Safe Browsing state.
TEST_F(SafetyCheckNotificationUtilsTest, ReturnsNothingForSafeState) {
  UNNotificationContent* safe_browsing_notification =
      NotificationForSafeBrowsingCheckState(
          SafeBrowsingSafetyCheckState::kSafe);

  EXPECT_EQ(safe_browsing_notification, nil);
}
