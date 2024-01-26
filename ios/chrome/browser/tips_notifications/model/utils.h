// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_

#import <UserNotifications/UserNotifications.h>

#import <optional>

namespace base {
class TimeDelta;
}

namespace tips_notifications {

// Identifier for the tips notification.
extern NSString* const kIdentifier;

// Key for tips notification type in UserInfo dictionary.
extern NSString* const kNotificationTypeKey;

// The type of Tips Notification, for an individual notification.
enum class NotificationType {
  kDefaultBrowser = 0,
  kWhatsNew = 1,
  kSignin = 2,
};

// The default amount of time after which a Tips notification is triggered.
extern const base::TimeDelta kDefaultTriggerDelta;

// Returns true if the given `notification` is a Tips notification.
bool IsTipsNotification(UNNotificationRequest* request);

// Returns a userInfo dictionary pre-filled with the notification `type`.
NSDictionary* UserInfoForType(NotificationType type);

// Returns the notification type found in a notification's userInfo dictionary.
std::optional<NotificationType> ParseType(UNNotificationRequest* request);

// Returns a newly generated notification request, with the given type and
// a trigger appropriate for a Tips notification.
UNNotificationRequest* Request(NotificationType type);

// Returns the notification content for a given Tips notification type.
UNNotificationContent* ContentForType(NotificationType type);

// Returns a trigger to be used when requesting a Tips notification.
UNNotificationTrigger* Trigger();

}  // namespace tips_notifications

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_
