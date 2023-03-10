// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_alert_presenter.h"

@class CommandDispatcher;

// Coordinator for Price Notifications, displaying the Price Notifications when
// starting.
@interface PriceNotificationsViewCoordinator
    : ChromeCoordinator <PriceNotificationsAlertPresenter>

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_COORDINATOR_H_
