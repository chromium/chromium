// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ProminenceNotificationSettingAlertCoordinatorDelegate;

// Coordinator for an alert that informs the user that notifications are being
// delivered silently.
@interface ProminenceNotificationSettingAlertCoordinator : ChromeCoordinator

@property(nonatomic, weak)
    id<ProminenceNotificationSettingAlertCoordinatorDelegate>
        delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_PROMINENCE_NOTIFICATION_SETTING_ALERT_COORDINATOR_H_
