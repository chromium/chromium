// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <string>

// Key of price tracking notification enabling status used in pref
// kContentNotificationsEnrollmentEligibility.
extern const char kPriceTrackingNotificationEnabledKey[];

// Key of user Feed engagement level used in pref
// kContentNotificationsEnrollmentEligibility.
extern const char kFeedActivityKey[];

// Key of user Chrome installed duration used in pref
// kContentNotificationsEnrollmentEligibility.
extern const char kNewUserKey[];

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONSTANTS_H_
