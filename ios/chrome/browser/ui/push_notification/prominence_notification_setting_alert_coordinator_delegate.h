// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_DELEGATE_H_

@class ProminenceNotificationSettingAlertCoordinator;

// Delegate for ProminenceNotificationSettingAlertCoordinator actions.
@protocol ProminenceNotificationSettingAlertCoordinatorDelegate

// Indicates to the delegate that this coordinator should be dismissed.
- (void)prominenceNotificationSettingAlertCoordinatorIsDone:
    (ProminenceNotificationSettingAlertCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_DELEGATE_H_
