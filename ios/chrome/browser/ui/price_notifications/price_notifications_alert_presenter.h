// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_ALERT_PRESENTER_H_

@class PriceNotificationsTableViewItem;

// Protocol for displaying Price Tracking related UIAlerts
@protocol PriceNotificationsAlertPresenter <NSObject>

// Displays the UIAlert that directs the user to the OS permission settings to
// enable push notification permissions.
- (void)presentPushNotificationPermissionAlert;

// Displays the UIAlert that indicates to the user that an error has occurred
// during the price tracking subscription process.
- (void)presentStartPriceTrackingErrorAlertForItem:
    (PriceNotificationsTableViewItem*)item;

// Displays the UIAlert that indicates to the user that an error has occurred
// during the price tracking subscription cancellation process.
- (void)presentStopPriceTrackingErrorAlertForItem:
    (PriceNotificationsTableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_ALERT_PRESENTER_H_
