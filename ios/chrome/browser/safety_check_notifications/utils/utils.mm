// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the corresponding number of insecure passwords for the given `state`
// using `insecure_password_counts`. If no insecure passwords are associated
// with the given `state`, returns `nil`.
NSNumber* InsecurePasswordCount(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  if (state == PasswordSafetyCheckState::kUnmutedCompromisedPasswords) {
    return [NSNumber numberWithInt:insecure_password_counts.compromised_count];
  }

  if (state == PasswordSafetyCheckState::kReusedPasswords) {
    return [NSNumber numberWithInt:insecure_password_counts.reused_count];
  }

  if (state == PasswordSafetyCheckState::kWeakPasswords) {
    return [NSNumber numberWithInt:insecure_password_counts.weak_count];
  }

  return nil;
}

// Returns a user info dictionary for a Password safety check notification.
// Contains the check result state, insecure password count, and a notification
// ID for logging.
NSDictionary* UserInfoForPasswordNotification(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  NSMutableDictionary* user_info =
      [NSMutableDictionary dictionaryWithDictionary:@{
        kSafetyCheckPasswordNotificationID : @YES,
        kSafetyCheckNotificationCheckResultKey :
            base::SysUTF8ToNSString(NameForSafetyCheckState(state))
      }];

  NSNumber* insecure_password_count =
      InsecurePasswordCount(state, insecure_password_counts);

  if (insecure_password_count) {
    user_info[kSafetyCheckNotificationInsecurePasswordCountKey] =
        insecure_password_count;
  }

  return user_info;
}

// Returns a user info dictionary for an Update Chrome safety check
// notification. Contains the check result state and a notification ID for
// logging.
NSDictionary* UserInfoForUpdateChromeNotification(
    UpdateChromeSafetyCheckState state) {
  return @{
    kSafetyCheckUpdateChromeNotificationID : @YES,
    kSafetyCheckNotificationCheckResultKey :
        base::SysUTF8ToNSString(NameForSafetyCheckState(state))
  };
}

// Returns a user info dictionary for a Safe Browsing safety check notification.
// Contains the check result state and a notification ID for logging.
NSDictionary* UserInfoForSafeBrowsingNotification(
    SafeBrowsingSafetyCheckState state) {
  return @{
    kSafetyCheckSafeBrowsingNotificationID : @YES,
    kSafetyCheckNotificationCheckResultKey :
        base::SysUTF8ToNSString(NameForSafetyCheckState(state))
  };
}

// Returns a notification content object with the provided `title`, `body`, and
// `user_info`. Includes a default notification sound.
UNNotificationContent* NotificationContent(NSString* title,
                                           NSString* body,
                                           NSDictionary* user_info) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];

  content.title = title;
  content.body = body;
  content.userInfo = user_info;
  content.sound = UNNotificationSound.defaultSound;

  return content;
}

}  // namespace

UNNotificationContent* NotificationForPasswordCheckState(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  if (state == PasswordSafetyCheckState::kUnmutedCompromisedPasswords) {
    CHECK_GT(insecure_password_counts.compromised_count, 0);

    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS),
        l10n_util::GetPluralNSStringF(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_COMPROMISED_PASSWORD,
            insecure_password_counts.compromised_count),
        UserInfoForPasswordNotification(state, insecure_password_counts));
  }

  if (state == PasswordSafetyCheckState::kReusedPasswords) {
    CHECK_GT(insecure_password_counts.reused_count, 0);

    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS),
        l10n_util::GetPluralNSStringF(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_REUSED_PASSWORD,
            insecure_password_counts.reused_count),
        UserInfoForPasswordNotification(state, insecure_password_counts));
  }

  if (state == PasswordSafetyCheckState::kWeakPasswords) {
    CHECK_GT(insecure_password_counts.weak_count, 0);

    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS),
        l10n_util::GetPluralNSStringF(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_WEAK_PASSWORD,
            insecure_password_counts.weak_count),
        UserInfoForPasswordNotification(state, insecure_password_counts));
  }

  return nil;
}

UNNotificationContent* NotificationForUpdateChromeCheckState(
    UpdateChromeSafetyCheckState state) {
  if (state == UpdateChromeSafetyCheckState::kOutOfDate) {
    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_UPDATE_BROWSER),
        l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME),
        UserInfoForUpdateChromeNotification(state));
  }

  return nil;
}

UNNotificationContent* NotificationForSafeBrowsingCheckState(
    SafeBrowsingSafetyCheckState state) {
  if (state == SafeBrowsingSafetyCheckState::kUnsafe) {
    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_ADD_BROWSING_PROTECTION),
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_BROWSING_PROTECTION_OFF),
        UserInfoForSafeBrowsingNotification(state));
  }

  return nil;
}
