// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_NOTIFICATION_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_NOTIFICATION_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"

@class UNNotification;
@class UNUserNotificationCenter;

enum class NotificationType;

// A protocol for an object that can classify a notification into a
// `NotificationType`.
@protocol NotificationClassifier

// Returns the type of the given `notification`.
- (NotificationType)classifyNotification:(UNNotification*)notification;

@end

// A class to record metrics related to notifications.
//
// This is a basic state tree, showing the 3 main histograms that can be
// recorded when a local notification is requested and triggers, or a remote
// notification is pushed:
//
//   UNNotificationCenter includes the notification in the "delivered" list
//    └──IOS.Notification.Received
//        ├──IOS.Notification.Dismissed
//        └──IOS.Notification.Interaction
//
// The app does not directly receive a signal when a notification is received
// or dismissed - it has to fetch the list of notifications from the
// notification center (using `getDeliveredNotifications`) and compare to the
// last time it fetched the list to see which ones are new. Then it can
// determine which ones are missing from the list since last time to determine
// which ones have either been dismissed or interacted with. It is possible for
// a notification to be received and dismissed before the app learns about it -
// in this case metrics cannot be recorded.
//
@interface NotificationMetricsRecorder : NSObject

// The object that will be used to classify notifications.
@property(nonatomic, weak) id<NotificationClassifier> classifier;

// Instantiates an instance with the passed-in notification center.
- (instancetype)initWithNotificationCenter:
    (UNUserNotificationCenter*)notificationCenter NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Checks for delivered notifications that have not yet been handled, and calls
// `closure` when finished.
- (void)handleDeliveredNotificationsWithClosure:(base::OnceClosure)closure;

// Returns YES if the given `notification` is in the "delivered" list from the
// notification center.
- (BOOL)wasDelivered:(UNNotification*)notification;

// Record that a notification was received.
- (void)recordReceived:(UNNotification*)notification;

// Record that a notification had an interaction.
- (void)recordInteraction:(UNNotification*)notification;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_NOTIFICATION_METRICS_RECORDER_H_
