// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <optional>

#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

class SafetyCheckNotificationUtilsTest : public PlatformTest {};

#pragma mark - Test cases

// Tests if notification content is correctly generated for the compromised
// password state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsCompromisedPasswordNotificationContentForCompromisedState) {
  // Define insecure password counts for the test case.
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 3,
                                                     /* dismissed */ 1,
                                                     /* reused */ 2,
                                                     /* weak */ 3};

  PasswordSafetyCheckState state =
      PasswordSafetyCheckState::kUnmutedCompromisedPasswords;

  // Call the function under test.
  UNNotificationContent* compromised_password_notification =
      NotificationForPasswordCheckState(state, counts);

  // Verify the notification content is not nil.
  EXPECT_NE(compromised_password_notification, nil);

  // Verify the title matches the expected string.
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);
  EXPECT_NSEQ(expected_title, compromised_password_notification.title);

  // Verify the body matches the expected plural string.
  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_COMPROMISED_PASSWORD,
      counts.compromised_count);
  EXPECT_NSEQ(expected_body, compromised_password_notification.body);

  // Verify user info contains the correct keys and values.
  EXPECT_TRUE([compromised_password_notification
                   .userInfo[kSafetyCheckPasswordNotificationID] boolValue]);

  // Verify the check result string matches the output of
  // NameForSafetyCheckState.
  EXPECT_NSEQ(compromised_password_notification
                  .userInfo[kSafetyCheckNotificationCheckResultKey],
              base::SysUTF8ToNSString(NameForSafetyCheckState(state)));

  EXPECT_EQ([compromised_password_notification
                    .userInfo[kSafetyCheckNotificationInsecurePasswordCountKey]
                intValue],
            counts.compromised_count);
}

// Tests if notification content is correctly generated for the weak password
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsWeakPasswordNotificationContentForWeakState) {
  // Define insecure password counts for the test case.
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 1,
                                                     /* dismissed */ 2,
                                                     /* reused */ 3,
                                                     /* weak */ 4};

  PasswordSafetyCheckState state = PasswordSafetyCheckState::kWeakPasswords;

  // Call the function under test.
  UNNotificationContent* weak_password_notification =
      NotificationForPasswordCheckState(state, counts);

  // Verify the notification content is not nil.
  EXPECT_NE(weak_password_notification, nil);

  // Verify the title matches the expected string.
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);
  EXPECT_NSEQ(expected_title, weak_password_notification.title);

  // Verify the body matches the expected plural string.
  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_WEAK_PASSWORD, counts.weak_count);
  EXPECT_NSEQ(expected_body, weak_password_notification.body);

  // Verify user info contains the correct keys and values.
  EXPECT_TRUE(
      [weak_password_notification.userInfo[kSafetyCheckPasswordNotificationID]
          boolValue]);

  // Verify the check result string matches the output of
  // NameForSafetyCheckState.
  EXPECT_NSEQ(weak_password_notification
                  .userInfo[kSafetyCheckNotificationCheckResultKey],
              base::SysUTF8ToNSString(NameForSafetyCheckState(state)));

  EXPECT_EQ([weak_password_notification
                    .userInfo[kSafetyCheckNotificationInsecurePasswordCountKey]
                intValue],
            counts.weak_count);
}

// Tests if notification content is correctly generated for the reused password
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsReusedPasswordNotificationContentForReusedState) {
  // Define insecure password counts for the test case.
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 2,
                                                     /* dismissed */ 3,
                                                     /* reused */ 5,
                                                     /* weak */ 4};

  PasswordSafetyCheckState state = PasswordSafetyCheckState::kReusedPasswords;

  // Call the function under test.
  UNNotificationContent* reused_password_notification =
      NotificationForPasswordCheckState(state, counts);

  // Verify the notification content is not nil.
  EXPECT_NE(reused_password_notification, nil);

  // Verify the title matches the expected string.
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);
  EXPECT_NSEQ(expected_title, reused_password_notification.title);

  // Verify the body matches the expected plural string.
  NSString* expected_body = l10n_util::GetPluralNSStringF(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_REUSED_PASSWORD, counts.reused_count);
  EXPECT_NSEQ(expected_body, reused_password_notification.body);

  // Verify user info contains the correct keys and values.
  EXPECT_TRUE(
      [reused_password_notification.userInfo[kSafetyCheckPasswordNotificationID]
          boolValue]);

  // Verify the check result string matches the output of
  // NameForSafetyCheckState.
  EXPECT_NSEQ(reused_password_notification
                  .userInfo[kSafetyCheckNotificationCheckResultKey],
              base::SysUTF8ToNSString(NameForSafetyCheckState(state)));

  EXPECT_EQ([reused_password_notification
                    .userInfo[kSafetyCheckNotificationInsecurePasswordCountKey]
                intValue],
            counts.reused_count);
}

// Tests that no notification content is generated for the safe password state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsNoPasswordNotificationContentForSafeState) {
  // Define insecure password counts for the test case (all zero).
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 0,
                                                     /* dismissed */ 0,
                                                     /* reused */ 0,
                                                     /* weak */ 0};

  PasswordSafetyCheckState state = PasswordSafetyCheckState::kSafe;

  // Call the function under test.
  UNNotificationContent* password_notification =
      NotificationForPasswordCheckState(state, counts);

  // Verify the notification content is nil.
  EXPECT_EQ(password_notification, nil);
}

// Tests if notification content is correctly generated for the out-of-date
// Chrome state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsUpdateChromeNotificationContentForOutOfDateState) {
  UpdateChromeSafetyCheckState state = UpdateChromeSafetyCheckState::kOutOfDate;

  // Call the function under test.
  UNNotificationContent* update_chrome_notification =
      NotificationForUpdateChromeCheckState(state);

  // Verify the notification content is not nil.
  EXPECT_NE(update_chrome_notification, nil);

  // Verify the title matches the expected string.
  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_UPDATE_BROWSER);
  EXPECT_NSEQ(expected_title, update_chrome_notification.title);

  // Verify the body matches the expected string.
  NSString* expected_body =
      l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
  EXPECT_NSEQ(expected_body, update_chrome_notification.body);

  // Verify user info contains the correct keys and values.
  EXPECT_TRUE(
      [update_chrome_notification
              .userInfo[kSafetyCheckUpdateChromeNotificationID] boolValue]);

  // Verify the check result string matches the output of
  // NameForSafetyCheckState.
  EXPECT_NSEQ(update_chrome_notification
                  .userInfo[kSafetyCheckNotificationCheckResultKey],
              base::SysUTF8ToNSString(NameForSafetyCheckState(state)));
}

// Tests that no notification content is generated for the up-to-date Chrome
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsNoUpdateChromeNotificationContentForUpToDateState) {
  UpdateChromeSafetyCheckState state = UpdateChromeSafetyCheckState::kUpToDate;
  // Call the function under test.
  UNNotificationContent* update_chrome_notification =
      NotificationForUpdateChromeCheckState(state);

  // Verify the notification content is nil.
  EXPECT_EQ(update_chrome_notification, nil);
}

// Tests if notification content is correctly generated for the unsafe Safe
// Browsing state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsSafeBrowsingNotificationContentForUnsafeState) {
  SafeBrowsingSafetyCheckState state = SafeBrowsingSafetyCheckState::kUnsafe;
  // Call the function under test.
  UNNotificationContent* safe_browsing_notification =
      NotificationForSafeBrowsingCheckState(state);

  // Verify the notification content is not nil.
  EXPECT_NE(safe_browsing_notification, nil);

  // Verify the title matches the expected string.
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_ADD_BROWSING_PROTECTION);
  EXPECT_NSEQ(expected_title, safe_browsing_notification.title);

  // Verify the body matches the expected string.
  NSString* expected_body = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_BROWSING_PROTECTION_OFF);
  EXPECT_NSEQ(expected_body, safe_browsing_notification.body);

  // Verify user info contains the correct keys and values.
  EXPECT_TRUE(
      [safe_browsing_notification
              .userInfo[kSafetyCheckSafeBrowsingNotificationID] boolValue]);
  // Verify the check result string matches the output of
  // NameForSafetyCheckState.
  EXPECT_NSEQ(safe_browsing_notification
                  .userInfo[kSafetyCheckNotificationCheckResultKey],
              base::SysUTF8ToNSString(NameForSafetyCheckState(state)));
}

// Tests that no notification content is generated for the safe Safe Browsing
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsNoSafeBrowsingNotificationContentForSafeState) {
  SafeBrowsingSafetyCheckState state = SafeBrowsingSafetyCheckState::kSafe;
  // Call the function under test.
  UNNotificationContent* safe_browsing_notification =
      NotificationForSafeBrowsingCheckState(state);

  // Verify the notification content is nil.
  EXPECT_EQ(safe_browsing_notification, nil);
}

// Tests that GetPasswordNotificationRequest returns a valid request for a
// compromised state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsPasswordRequestForCompromisedState) {
  // Define insecure password counts.
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 3,
                                                     /* dismissed */ 1,
                                                     /* reused */ 2,
                                                     /* weak */ 3};
  PasswordSafetyCheckState state =
      PasswordSafetyCheckState::kUnmutedCompromisedPasswords;

  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetPasswordNotificationRequest(state, counts);

  // Verify a request is returned.
  ASSERT_TRUE(request.has_value());
  // Verify the identifier is correct.
  EXPECT_NSEQ(kSafetyCheckPasswordNotificationID, request.value().identifier);
  // Verify the content is not nil.
  EXPECT_NE(nil, request.value().content);
  // Verify the time interval is positive.
  EXPECT_GT(request.value().time_interval.InSeconds(), 0);
  // Verify content details (spot check title).
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS);
  EXPECT_NSEQ(expected_title, request.value().content.title);
  // Verify userInfo check result (spot check).
  EXPECT_NSEQ(
      request.value().content.userInfo[kSafetyCheckNotificationCheckResultKey],
      base::SysUTF8ToNSString(NameForSafetyCheckState(state)));
}

// Tests that GetPasswordNotificationRequest returns nullopt for a safe state.
TEST_F(SafetyCheckNotificationUtilsTest, ReturnsNoPasswordRequestForSafeState) {
  // Define insecure password counts (all zero).
  password_manager::InsecurePasswordCounts counts = {/* compromised */ 0,
                                                     /* dismissed */ 0,
                                                     /* reused */ 0,
                                                     /* weak */ 0};
  PasswordSafetyCheckState state = PasswordSafetyCheckState::kSafe;
  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetPasswordNotificationRequest(state, counts);

  // Verify no request is returned.
  EXPECT_FALSE(request.has_value());
}

// Tests that GetUpdateChromeNotificationRequest returns a valid request for an
// out-of-date state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsUpdateChromeRequestForOutOfDateState) {
  UpdateChromeSafetyCheckState state = UpdateChromeSafetyCheckState::kOutOfDate;
  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetUpdateChromeNotificationRequest(state);

  // Verify a request is returned.
  ASSERT_TRUE(request.has_value());
  // Verify the identifier is correct.
  EXPECT_NSEQ(kSafetyCheckUpdateChromeNotificationID,
              request.value().identifier);
  // Verify the content is not nil.
  EXPECT_NE(nil, request.value().content);
  // Verify the time interval is positive.
  EXPECT_GT(request.value().time_interval.InSeconds(), 0);
  // Verify content details (spot check title).
  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_UPDATE_BROWSER);
  EXPECT_NSEQ(expected_title, request.value().content.title);
  // Verify userInfo check result (spot check).
  EXPECT_NSEQ(
      request.value().content.userInfo[kSafetyCheckNotificationCheckResultKey],
      base::SysUTF8ToNSString(NameForSafetyCheckState(state)));
}

// Tests that GetUpdateChromeNotificationRequest returns nullopt for an
// up-to-date state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsNoUpdateChromeRequestForUpToDateState) {
  UpdateChromeSafetyCheckState state = UpdateChromeSafetyCheckState::kUpToDate;
  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetUpdateChromeNotificationRequest(state);

  // Verify no request is returned.
  EXPECT_FALSE(request.has_value());
}

// Tests that GetSafeBrowsingNotificationRequest returns a valid request for an
// unsafe state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsSafeBrowsingRequestForUnsafeState) {
  SafeBrowsingSafetyCheckState state = SafeBrowsingSafetyCheckState::kUnsafe;
  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetSafeBrowsingNotificationRequest(state);

  // Verify a request is returned.
  ASSERT_TRUE(request.has_value());
  // Verify the identifier is correct.
  EXPECT_NSEQ(kSafetyCheckSafeBrowsingNotificationID,
              request.value().identifier);
  // Verify the content is not nil.
  EXPECT_NE(nil, request.value().content);
  // Verify the time interval is positive.
  EXPECT_GT(request.value().time_interval.InSeconds(), 0);
  // Verify content details (spot check title).
  NSString* expected_title = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_ADD_BROWSING_PROTECTION);
  EXPECT_NSEQ(expected_title, request.value().content.title);
  // Verify userInfo check result (spot check).
  EXPECT_NSEQ(
      request.value().content.userInfo[kSafetyCheckNotificationCheckResultKey],
      base::SysUTF8ToNSString(NameForSafetyCheckState(state)));
}

// Tests that GetSafeBrowsingNotificationRequest returns nullopt for a safe
// state.
TEST_F(SafetyCheckNotificationUtilsTest,
       ReturnsNoSafeBrowsingRequestForSafeState) {
  SafeBrowsingSafetyCheckState state = SafeBrowsingSafetyCheckState::kSafe;
  // Call the function under test.
  std::optional<ScheduledNotificationRequest> request =
      GetSafeBrowsingNotificationRequest(state);

  // Verify no request is returned.
  EXPECT_FALSE(request.has_value());
}

#pragma mark - ParseSafetyCheckNotificationType Tests

// Tests that a request without a Safety Check-related notification identifier
// is correctly identified as NOT a Safety Check notification.
TEST_F(SafetyCheckNotificationUtilsTest, IdentifiesNonSafetyCheckRequest) {
  // Create a mock notification content.
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = @"Not Safety Check";

  // Create a mock notification request with a non-safety check identifier.
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:@"NOT_SAFETY_CHECK"
                                           content:content
                                           trigger:nil];

  // Verify the parsing function returns nullopt.
  EXPECT_FALSE(ParseSafetyCheckNotificationType(request).has_value());
}

// Tests that an Update Chrome notification request is correctly identified and
// returns the `kUpdateChrome` type.
TEST_F(SafetyCheckNotificationUtilsTest, IdentifiesUpdateChromeRequest) {
  // Create a mock notification content.
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = @"Safety Check";

  // Create a mock notification request with the Update Chrome identifier.
  UNNotificationRequest* request = [UNNotificationRequest
      requestWithIdentifier:kSafetyCheckUpdateChromeNotificationID
                    content:content
                    trigger:nil];

  // Verify the parsing function returns the correct type.
  std::optional<SafetyCheckNotificationType> type =
      ParseSafetyCheckNotificationType(request);
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(type.value(), SafetyCheckNotificationType::kUpdateChrome);
}

// Tests that a Passwords notification request is correctly identified and
// returns the `kPasswords` type.
TEST_F(SafetyCheckNotificationUtilsTest, IdentifiesPasswordRequest) {
  // Create a mock notification content.
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = @"Safety Check";

  // Create a mock notification request with the Password identifier.
  UNNotificationRequest* request = [UNNotificationRequest
      requestWithIdentifier:kSafetyCheckPasswordNotificationID
                    content:content
                    trigger:nil];

  // Verify the parsing function returns the correct type.
  std::optional<SafetyCheckNotificationType> type =
      ParseSafetyCheckNotificationType(request);
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(type.value(), SafetyCheckNotificationType::kPasswords);
}

// Tests that a Safe Browsing notification request is correctly identified and
// returns the `kSafeBrowsing` type.
TEST_F(SafetyCheckNotificationUtilsTest, IdentifiesSafeBrowsingRequest) {
  // Create a mock notification content.
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = @"Safety Check";

  // Create a mock notification request with the Safe Browsing identifier.
  UNNotificationRequest* request = [UNNotificationRequest
      requestWithIdentifier:kSafetyCheckSafeBrowsingNotificationID
                    content:content
                    trigger:nil];

  // Verify the parsing function returns the correct type.
  std::optional<SafetyCheckNotificationType> type =
      ParseSafetyCheckNotificationType(request);
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(type.value(), SafetyCheckNotificationType::kSafeBrowsing);
}
