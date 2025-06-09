// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_interaction_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// View controller that presents a half-sheet UI allowing users to set reminders
// for tabs they want to revisit later. Uses the confirmation alert style to
// present a title, descriptive text, bell icon, and action buttons.
@interface ReminderNotificationsViewController
    : ConfirmationAlertViewController <
          ReminderNotificationsDatePickerInteractionHandler>

// The currently selected date and time for the reminder notification.
// Default value is the current date and time when the picker is initialized.
// Updates when the user selects a new date or time.
//
// Note: This always represents a future date as users cannot select past dates.
@property(nonatomic, readonly) NSDate* date;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_VIEW_CONTROLLER_H_
