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

// Identifier for the tips notification.
extern NSString* const kTipsNotificationId;

// Key for tips notification type in UserInfo dictionary.
extern NSString* const kNotificationTypeKey;

// Pref that stores which notifications have been sent.
extern const char kTipsNotificationsSentPref[];

// The type of Tips Notification, for an individual notification.
enum class TipsNotificationType {
  kDefaultBrowser = 0,
  kWhatsNew = 1,
  kSignin = 2,
};

// The default amount of time after which a Tips notification is triggered.
extern const base::TimeDelta kTipsNotificationDefaultTriggerDelta;

// Returns true if the given `notification` is a Tips notification.
bool IsTipsNotification(UNNotificationRequest* request);

// Returns a userInfo dictionary pre-filled with the notification `type`.
NSDictionary* UserInfoForTipsNotificationType(TipsNotificationType type);

// Returns the notification type found in a notification's userInfo dictionary.
std::optional<TipsNotificationType> ParseTipsNotificationType(
    UNNotificationRequest* request);

// Returns a newly generated notification request, with the given type and
// a trigger appropriate for a Tips notification.
UNNotificationRequest* TipsNotificationRequest(TipsNotificationType type);

// Returns the notification content for a given Tips notification type.
UNNotificationContent* ContentForTipsNotificationType(
    TipsNotificationType type);

// Returns a trigger to be used when requesting a Tips notification.
UNNotificationTrigger* TipsNotificationTrigger();

// Returns a bitfield indicating which types of notifications should be
// enabled. Bits are assigned based on the enum `TipsNotificationType`.
int TipsNotificationsEnabledBitfield();

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_
