// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_CONFIRMATION_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_CONFIRMATION_PRESENTER_H_

// Protocol for displaying Notification related UIAlerts.
@protocol NotificationsConfirmationPresenter <NSObject>

// Displays the snackbar message when the user successfully opt-in to the
// notification.
- (void)presentNotificationsConfirmationMessage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_CONFIRMATION_PRESENTER_H_
