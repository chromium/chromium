// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/utils.h"

#import "base/time/time.h"

namespace tips_notifications {

NSString* const kIdentifier = @"kTipsNotificationId";
NSString* const kNotificationTypeKey = @"kNotificationTypeKey";
const base::TimeDelta kDefaultTriggerDelta = base::Hours(72);

bool IsTipsNotification(UNNotificationRequest* request) {
  return [request.identifier isEqualToString:kIdentifier];
}

NSDictionary* UserInfoForType(NotificationType type) {
  return @{
    kIdentifier : @YES,
    kNotificationTypeKey : @(static_cast<int>(type)),
  };
}

std::optional<NotificationType> ParseType(UNNotificationRequest* request) {
  NSDictionary* user_info = request.content.userInfo;
  NSNumber* type = user_info[kNotificationTypeKey];
  if (type == nil) {
    return std::nullopt;
  }
  return static_cast<NotificationType>(type.integerValue);
}

UNNotificationRequest* Request(NotificationType type) {
  return [UNNotificationRequest requestWithIdentifier:kIdentifier
                                              content:ContentForType(type)
                                              trigger:Trigger()];
}

UNNotificationContent* ContentForType(NotificationType type) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  // TODO(crbug.com/1517919): Add the real notification content.
  content.title = @"Tips Notification";
  content.body = @"This notification was triggered by an elapsed time trigger.";
  content.userInfo = UserInfoForType(type);
  return content;
}

UNNotificationTrigger* Trigger() {
  return [UNTimeIntervalNotificationTrigger
      triggerWithTimeInterval:kDefaultTriggerDelta.InSecondsF()
                      repeats:NO];
}

}  // namespace tips_notifications
