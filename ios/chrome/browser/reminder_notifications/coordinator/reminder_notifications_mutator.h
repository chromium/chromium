// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MUTATOR_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MUTATOR_H_

#import <UIKit/UIKit.h>

namespace base {
class Time;
}
class GURL;

// `ReminderNotificationsMutator` protocol defines the interface for the UI
// layer to communicate with the `ReminderNotificationsMediator`.
@protocol ReminderNotificationsMutator

// Sets a reminder for the given `URL` at the specified `time`.
- (void)setReminderForURL:(const GURL&)URL time:(base::Time)time;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MUTATOR_H_
