// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_

#import <UserNotifications/UserNotifications.h>

#import <optional>
#import <vector>

namespace base {
class TimeDelta;
}

// Identifier for the tips notification.
extern NSString* const kTipsNotificationId;

// Key for tips notification type in UserInfo dictionary.
extern NSString* const kNotificationTypeKey;

// Pref that stores which notifications have been sent.
extern const char kTipsNotificationsSentPref[];

// Pref that stores which notification type was sent last.
extern const char kTipsNotificationsLastSent[];

// Pref that stores which notification type was triggered last.
extern const char kTipsNotificationsLastTriggered[];

// Pref that stores the last time that a notification was requested.
extern const char kTipsNotificationsLastRequestedTime[];

// Pref that stores the user's classification.
extern const char kTipsNotificationsUserType[];

// Pref that stores how many Tips notifications have been dismissed in a row.
extern const char kTipsNotificationsDismissCount[];

// The type of Tips Notification, for an individual notification.
// Always keep this enum in sync with
// the corresponding IOSTipsNotificationType in enums.xml.
// LINT.IfChange
enum class TipsNotificationType {
  kDefaultBrowser = 0,
  kWhatsNew = 1,
  kSignin = 2,
  kError = 3,
  kSetUpListContinuation = 4,
  kDocking = 5,
  kOmniboxPosition = 6,
  kLens = 7,
  kEnhancedSafeBrowsing = 8,
  kMaxValue = kEnhancedSafeBrowsing,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// An enum to store a classification of Tips Notification users.
// LINT.IfChange
enum class TipsNotificationUserType {
  kUnknown = 0,
  kLessEngaged = 1,
  kActiveSeeker = 2,
  kMaxValue = kActiveSeeker,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

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
UNNotificationRequest* TipsNotificationRequest(
    TipsNotificationType type,
    TipsNotificationUserType user_type);

// Returns the notification content for a given Tips notification type.
UNNotificationContent* ContentForTipsNotificationType(
    TipsNotificationType type);

// Returns the time delta used to trigger Tips notifications.
base::TimeDelta TipsNotificationTriggerDelta(
    TipsNotificationUserType user_type);

// Returns a trigger to be used when requesting a Tips notification.
UNNotificationTrigger* TipsNotificationTrigger(
    TipsNotificationUserType user_type);

// Returns a bitfield indicating which types of notifications should be
// enabled. Bits are assigned based on the enum `TipsNotificationType`.
int TipsNotificationsEnabledBitfield();

// Returns an ordered array containing the types of Tips Notifications to send.
std::vector<TipsNotificationType> TipsNotificationsTypesOrder();

// Returns the dismiss limit. If the user dismisses this number of Tips
// notifications in a row, no more Tips notifications will be sent. Zero
// indicates there should be no limit.
int TipsNotificationsDismissLimit();

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_UTILS_H_
