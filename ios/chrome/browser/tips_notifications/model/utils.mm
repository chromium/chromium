// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/utils.h"

#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Holds the l10n string ids for tips notification content.
struct ContentIDs {
  int title;
  int body;
};

// Returns the ContentIDs for the given `type`.
ContentIDs ContentIDsForType(TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return {IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_BODY};
    case TipsNotificationType::kWhatsNew:
      return {IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_BODY};
    case TipsNotificationType::kSignin:
      return {IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_BODY};
    case TipsNotificationType::kError:
      NOTREACHED_NORETURN();
  }
}

// A bitfield with all notification types enabled.
const int kEnableAllNotifications = 7;

}  // namespace

NSString* const kTipsNotificationId = @"kTipsNotificationId";
NSString* const kTipsNotificationTypeKey = @"kTipsNotificationTypeKey";
const base::TimeDelta kTipsNotificationDefaultTriggerDelta = base::Hours(72);
const char kTipsNotificationsSentPref[] = "tips_notifications.sent_bitfield";
const char kTipsNotificationsLastSent[] = "tips_notifiations.last_sent";
const char kTipsNotificationsLastTriggered[] =
    "tips_notifiations.last_triggered";

bool IsTipsNotification(UNNotificationRequest* request) {
  return [request.identifier isEqualToString:kTipsNotificationId];
}

NSDictionary* UserInfoForTipsNotificationType(TipsNotificationType type) {
  return @{
    kTipsNotificationId : @YES,
    kTipsNotificationTypeKey : @(static_cast<int>(type)),
  };
}

std::optional<TipsNotificationType> ParseTipsNotificationType(
    UNNotificationRequest* request) {
  NSDictionary* user_info = request.content.userInfo;
  NSNumber* type = user_info[kTipsNotificationTypeKey];
  if (type == nil) {
    return std::nullopt;
  }
  return static_cast<TipsNotificationType>(type.integerValue);
}

UNNotificationRequest* TipsNotificationRequest(TipsNotificationType type) {
  return [UNNotificationRequest
      requestWithIdentifier:kTipsNotificationId
                    content:ContentForTipsNotificationType(type)
                    trigger:TipsNotificationTrigger()];
}

UNNotificationContent* ContentForTipsNotificationType(
    TipsNotificationType type) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  ContentIDs content_ids = ContentIDsForType(type);
  content.title = l10n_util::GetNSString(content_ids.title);
  content.body = l10n_util::GetNSString(content_ids.body);
  content.userInfo = UserInfoForTipsNotificationType(type);
  content.sound = UNNotificationSound.defaultSound;
  return content;
}

UNNotificationTrigger* TipsNotificationTrigger() {
  NSTimeInterval trigger_interval =
      GetFieldTrialParamByFeatureAsTimeDelta(
          kIOSTipsNotifications, kIOSTipsNotificationsTriggerTimeParam,
          kTipsNotificationDefaultTriggerDelta)
          .InSecondsF();
  return [UNTimeIntervalNotificationTrigger
      triggerWithTimeInterval:trigger_interval
                      repeats:NO];
}

int TipsNotificationsEnabledBitfield() {
  return GetFieldTrialParamByFeatureAsInt(kIOSTipsNotifications,
                                          kIOSTipsNotificationsEnabledParam,
                                          kEnableAllNotifications);
}
