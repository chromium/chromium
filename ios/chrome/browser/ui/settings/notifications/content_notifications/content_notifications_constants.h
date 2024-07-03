// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The action that has occurred from the settings toggle for Content
// Notifications.
// LINT.IfChange
enum class ContentNotificationSettingsToggleAction {
  kEnabledContent = 0,
  kDisabledContent = 1,
  kEnabledSports = 2,
  kDisabledSports = 3,
  kMaxValue = kDisabledSports,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/content/enums.xml)

// The accessibility identifier of the Content Notifications setting table view.
extern NSString* const kContentNotificationsTableViewId;

// The accessibility identifier of the Content Notifications cell.
extern NSString* const kContentNotificationsCellId;

// The accessibility identifier of the Sports notifications cell.
extern NSString* const kSportsNotificationsCellId;

// The SF Symbol for the medal.
extern NSString* const kMedalSymbol;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSTANTS_H_
