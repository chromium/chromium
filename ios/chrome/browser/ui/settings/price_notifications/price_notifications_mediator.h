// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_view_controller_delegate.h"

@protocol PriceNotificationsConsumer;
@protocol PriceNotificationsNavigationCommands;

// Mediator for Price Notifications.
@interface PriceNotificationsMediator
    : NSObject <PriceNotificationsViewControllerDelegate>

// View controller.
@property(nonatomic, weak) id<PriceNotificationsConsumer> consumer;

// Handler used to navigate inside the Price Notifications setting.
@property(nonatomic, weak) id<PriceNotificationsNavigationCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_MEDIATOR_H_
