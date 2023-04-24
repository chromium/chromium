// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_ALERT_PRESENTER_H_

@class PriceNotificationsTableViewItem;

// Protocol for displaying Price Tracking related UIAlerts
@protocol TrackingPriceAlertPresenter <NSObject>

// Displays the UIAlert that directs the user to the OS permission settings to
// enable push notification permissions when the user toggles Chrome-level push
// notification permissions for price tracking.
- (void)presentPushNotificationPermissionAlert;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_ALERT_PRESENTER_H_
