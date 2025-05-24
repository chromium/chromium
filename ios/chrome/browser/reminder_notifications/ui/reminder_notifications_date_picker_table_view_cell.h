// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_TABLE_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_TABLE_VIEW_CELL_H_

#import <UIKit/UIKit.h>

// A custom table view cell used to display date and time picker rows in the
// reminder notifications UI. Contains a label for the field name (Date/Time)
// and a value label displaying the selected date or time.
@interface ReminderNotificationsDatePickerTableViewCell : UITableViewCell

// Configures the cell with a label and value.
- (void)configureWithLabel:(NSString*)labelText value:(NSString*)valueText;

@end

#endif  // IOS_CHROME_BROWSER_REMINDER_NOTIFICATIONS_UI_REMINDER_NOTIFICATIONS_DATE_PICKER_TABLE_VIEW_CELL_H_
