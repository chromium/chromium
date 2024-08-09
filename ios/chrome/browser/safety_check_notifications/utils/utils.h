// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_UTILS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_UTILS_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

// Returns notification content for the most critical Password issue found using
// `state` and `insecure_password_counts`. Returns `nil` if no notification
// content can be created, i.e., no issue is found.
UNNotificationContent* NotificationForPasswordCheckState(
    PasswordSafetyCheckState state,
    password_manager::InsecurePasswordCounts insecure_password_counts);

// Returns notification content for the most critical Update Chrome issue found
// using `state`. Returns `nil` if no notification content can be created, i.e.,
// no issue is found.
UNNotificationContent* NotificationForUpdateChromeCheckState(
    UpdateChromeSafetyCheckState state);

// Returns notification content for the most critical Safe Browsing issue found
// using `state`. Returns `nil` if no notification content can be created, i.e.,
// no issue is found.
UNNotificationContent* NotificationForSafeBrowsingCheckState(
    SafeBrowsingSafetyCheckState state);

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_NOTIFICATIONS_UTILS_UTILS_H_
