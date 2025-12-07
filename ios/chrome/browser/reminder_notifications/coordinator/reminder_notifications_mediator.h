// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mutator.h"

class PrefService;

// Keys for the reminder notifications dictionary stored in Prefs.
extern const char kReminderNotificationsTimeKey[];
extern const char kReminderNotificationsCreationTimeKey[];

// `ReminderNotificationsMediator` is responsible for handling Reminder
// Notification data storage.
@interface ReminderNotificationsMediator
    : NSObject <ReminderNotificationsMutator>

// Initializes the mediator with `profilePrefs`.
- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefs
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator, releasing dependencies. Called when the mediator is
// no longer needed.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_COORDINATOR_REMINDER_NOTIFICATIONS_MEDIATOR_H_
