// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
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

// Base helper for creating each Safety Check notification's userInfo
// dictionary.
template <typename SafetyCheckStateType>
NSMutableDictionary* BaseUserInfoForNotification(NSString* type_id_key,
                                                 SafetyCheckStateType state) {
  return [@{
    type_id_key : @YES,
    kSafetyCheckNotificationCheckResultKey :
        base::SysUTF8ToNSString(NameForSafetyCheckState(state))
  } mutableCopy];
}

// Returns a user info dictionary for a Password safety check notification.
// Contains the check result state, insecure password count, and a notification
// ID for logging.
NSDictionary* UserInfoForPasswordNotification(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  NSMutableDictionary* user_info =
      BaseUserInfoForNotification(kSafetyCheckPasswordNotificationID, state);

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
  return BaseUserInfoForNotification(kSafetyCheckUpdateChromeNotificationID,
                                     state);
}

// Returns a user info dictionary for a Safe Browsing safety check notification.
// Contains the check result state and a notification ID for logging.
NSDictionary* UserInfoForSafeBrowsingNotification(
    SafeBrowsingSafetyCheckState state) {
  return BaseUserInfoForNotification(kSafetyCheckSafeBrowsingNotificationID,
                                     state);
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

// Returns the time duration of user inactivity required before Safety Check
// notifications are triggered. This duration can be modified through
// Experimental settings or Finch. If no override is set, a default value is
// used.
base::TimeDelta InactiveThresholdForNotifications() {
  // Check if an override is set via experimental flags.
  std::optional<int> forced_threshold_seconds = experimental_flags::
      GetForcedInactivityThresholdForSafetyCheckNotifications();

  if (forced_threshold_seconds.has_value()) {
    return base::Seconds(forced_threshold_seconds.value());
  }

  return InactiveThresholdForSafetyCheckNotifications();
}

// Helper to create the final scheduled request if `content` is valid.
std::optional<ScheduledNotificationRequest> CreateScheduledNotificationRequest(
    UNNotificationContent* content,
    NSString* identifier) {
  if (!content) {
    return std::nullopt;
  }

  return ScheduledNotificationRequest{
      .identifier = identifier,
      .content = content,
      .time_interval = InactiveThresholdForNotifications()};
}

}  // namespace

void LogSafetyCheckNotificationOptInSource(
    SafetyCheckNotificationsOptInSource opt_in_source,
    SafetyCheckNotificationsOptInSource opt_out_source) {
  bool is_notifications_enabled = push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kSafetyCheck, GaiaId());

  base::UmaHistogramEnumeration(
      "IOS.Notifications.SafetyCheck.NotificationsOptInSource",
      is_notifications_enabled ? opt_out_source : opt_in_source);
}

std::optional<ScheduledNotificationRequest> GetPasswordNotificationRequest(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  UNNotificationContent* content =
      NotificationForPasswordCheckState(state, insecure_password_counts);

  return CreateScheduledNotificationRequest(content,
                                            kSafetyCheckPasswordNotificationID);
}

UNNotificationContent* NotificationForPasswordCheckState(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts) {
  if (state == PasswordSafetyCheckState::kUnmutedCompromisedPasswords &&
      insecure_password_counts.compromised_count > 0) {
    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS),
        l10n_util::GetPluralNSStringF(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_COMPROMISED_PASSWORD,
            insecure_password_counts.compromised_count),
        UserInfoForPasswordNotification(state, insecure_password_counts));
  }

  if (state == PasswordSafetyCheckState::kReusedPasswords &&
      insecure_password_counts.reused_count > 0) {
    return NotificationContent(
        l10n_util::GetNSString(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_CHANGE_PASSWORDS),
        l10n_util::GetPluralNSStringF(
            IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_REUSED_PASSWORD,
            insecure_password_counts.reused_count),
        UserInfoForPasswordNotification(state, insecure_password_counts));
  }

  if (state == PasswordSafetyCheckState::kWeakPasswords &&
      insecure_password_counts.weak_count > 0) {
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

std::optional<ScheduledNotificationRequest> GetUpdateChromeNotificationRequest(
    UpdateChromeSafetyCheckState state) {
  UNNotificationContent* content = NotificationForUpdateChromeCheckState(state);

  return CreateScheduledNotificationRequest(
      content, kSafetyCheckUpdateChromeNotificationID);
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

std::optional<ScheduledNotificationRequest> GetSafeBrowsingNotificationRequest(
    SafeBrowsingSafetyCheckState state) {
  UNNotificationContent* content = NotificationForSafeBrowsingCheckState(state);

  return CreateScheduledNotificationRequest(
      content, kSafetyCheckSafeBrowsingNotificationID);
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

std::optional<SafetyCheckNotificationType> ParseSafetyCheckNotificationType(
    UNNotificationRequest* request) {
  if ([request.identifier
          isEqualToString:kSafetyCheckUpdateChromeNotificationID]) {
    return SafetyCheckNotificationType::kUpdateChrome;
  }

  if ([request.identifier isEqualToString:kSafetyCheckPasswordNotificationID]) {
    return SafetyCheckNotificationType::kPasswords;
  }

  if ([request.identifier
          isEqualToString:kSafetyCheckSafeBrowsingNotificationID]) {
    return SafetyCheckNotificationType::kSafeBrowsing;
  }

  return std::nullopt;
}
