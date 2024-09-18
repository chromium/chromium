// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Enum for the `IOS.Notifications.SafetyCheck.Interaction` histogram.
//
// Must be in sync with `IOSSafetyCheckNotificationType` enum in
// `tools/metrics/histograms/metadata/ios/enums.xml`.
//
// LINT.IfChange(SafetyCheckNotificationType)
enum class SafetyCheckNotificationType {
  kUpdateChrome = 0,
  kPasswords = 1,
  kSafeBrowsing = 2,
  kError = 3,
  kMaxValue = kError,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSafetyCheckNotificationType)

// The default duration of user inactivity required before displaying a Safety
// Check push notification.
inline constexpr base::TimeDelta kSafetyCheckNotificationDefaultDelay =
    base::Hours(24);

// Unique identifiers and keys for Safety Check push notifications.

// Notification ID for the current Password notification.
extern NSString* const kSafetyCheckPasswordNotificationID;

// Notification ID for the current Update Chrome notification.
extern NSString* const kSafetyCheckUpdateChromeNotificationID;

// Notification ID for the current Safe Browsing notification.
extern NSString* const kSafetyCheckSafeBrowsingNotificationID;

// Key for the Safety Check result (string) stored in the notification's
// `UserInfo`.
extern NSString* const kSafetyCheckNotificationCheckResultKey;

// Key for the count of insecure passwords (integer) stored in the current
// Password notification's `UserInfo`.
extern NSString* const kSafetyCheckNotificationInsecurePasswordCountKey;

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_CONSTANTS_H_
