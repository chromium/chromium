// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <string>

// Enum for the NAU implementation for Content notifications. Change
// NotificationActionType enum when this one changes.
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

// Enum for the NAU implementation for Content notifications used for metrics.
enum class NotificationActionType {
  // When a content notification is displayed on the device.
  kNotificationActionTypeDisplayed = NAUActionTypeDisplayed,
  // When a content notification is opened.
  kNotificationActionTypeOpened = NAUActionTypeOpened,
  // When a content notification is dismissed.
  kNotificationActionTypeDismissed = NAUActionTypeDismissed,
  // When the feedback secondary action is triggered.
  kNotificationActionTypeFeedbackClicked = NAUActionTypeFeedbackClicked,
  // Max value.
  kMaxValue = kNotificationActionTypeFeedbackClicked,
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
// `kFeaturePushNotificationPermissions`.
extern const char kCommerceNotificationKey[];

// Key of content notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kContentNotificationKey[];

// Key of sports notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSportsNotificationKey[];

// Key of tips notification used in pref `kFeaturePushNotificationPermissions`.
extern const char kTipsNotificationKey[];

// Key of send tab notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSendTabNotificationKey[];

// Key of Safety Check notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSafetyCheckNotificationKey[];

// Action identifier for the Content Notifications Feedback action.
extern NSString* const kContentNotificationFeedbackActionIdentifier;

// Category identifier for the Content Notifications category that contains a
// Feedback action.
extern NSString* const kContentNotificationFeedbackCategoryIdentifier;

// The body parameter of the notification for a Content Notification delivered
// NAU.
extern NSString* const kContentNotificationNAUBodyParameter;

// The NSUserDefaults key for the delivered content notifications that need to
// be reported.
extern NSString* const kContentNotificationContentArrayKey;

// The histogram name for the NAU success metric.
extern const char kNAUHistogramName[];

// The histogram name to record a Content Notification action.
extern const char kContentNotificationActionHistogramName[];

// The max amount of NAU sends per session.
extern const int kDeliveredNAUMaxSendsPerSession;

// Key for the Push Notification Client Id type in notification payload. Used
// for Send Tab notifications.
extern NSString* const kPushNotificationClientIdKey;

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
