// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_INTERACTION_HANDLER_H_

#import <UIKit/UIKit.h>

// Protocol for handling user interactions with the reminder notifications date
// picker.
@protocol ReminderNotificationsDatePickerInteractionHandler <NSObject>

// Called when the date (or time) row is selected in the date picker.
- (void)showDatePickerFromOriginView:(UIView*)view
                  withDatePickerMode:(UIDatePickerMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_INTERACTION_HANDLER_H_
