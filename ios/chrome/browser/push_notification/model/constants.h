// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <string>

// Enum for the NAU implementation for Content notifications.
typedef NS_ENUM(NSInteger, NAUActionType) {
  // When a content notification is displayed on the device.
  NAUActionTypeDisplayed = 0,
  // When a content notification is opened.
  NAUActionTypeOpened = 1,
  // When a content notification is dismissed.
  NAUActionTypeDismissed = 2,
  // When the feedback secondary action is triggered.
  NAUActionTypeFeedbackClicked = 3,
};

// Enum for the NAU implementation for Content notifications.
typedef NS_ENUM(NSInteger, SettingsToggleType) {
  // None of the toggles has changed.
  SettingsToggleTypeNone = 0,
  // The settings toggle identifier for Content for NAU.
  SettingsToggleTypeContent = 1,
  // The settings toggle identifier for sports for NAU.
  SettingsToggleTypeSports = 2,
};

// Key of commerce notification used in pref
// kFeaturePushNotificationPermissions.
extern const char kCommerceNotificationKey[];

// Key of content notification used in pref kFeaturePushNotificationPermissions.
extern const char kContentNotificationKey[];

// Key of sports notification used in pref kFeaturePushNotificationPermissions.
extern const char kSportsNotificationKey[];

// Key of tips notification used in pref kFeaturePushNotificationPermissions.
extern const char kTipsNotificationKey[];

// Action identifier for the Content Notifications Feedback action.
extern NSString* const kContentNotificationFeedbackActionIdentifier;

// Category identifier for the Content Notifications category that contains a
// Feedback action.
extern NSString* const kContentNotificationFeedbackCategoryIdentifier;

// The body parameter of the notification for a Content Notification delivered
// NAU.
extern NSString* const kContentNotificationNAUBodyParameter;

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
