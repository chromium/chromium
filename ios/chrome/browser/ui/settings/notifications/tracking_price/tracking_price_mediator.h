// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_view_controller_delegate.h"

class AuthenticationService;
class PrefService;
@class TableViewSwitchItem;
@protocol TrackingPriceAlertPresenter;
@protocol TrackingPriceConsumer;

namespace commerce {
class ShoppingService;
}  // namespace commerce

// Mediator for the Tracking Price.
@interface TrackingPriceMediator
    : NSObject <TrackingPriceViewControllerDelegate>

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
      authenticationService:(AuthenticationService*)authenticationService
                prefService:(PrefService*)prefService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// View controller.
@property(nonatomic, weak) id<TrackingPriceConsumer> consumer;

// Mobile notification item.
@property(nonatomic, strong) TableViewSwitchItem* mobileNotificationItem;

// Email notification item.
@property(nonatomic, strong) TableViewSwitchItem* emailNotificationItem;

// Handler for displaying price tracking related alerts.
@property(nonatomic, weak) id<TrackingPriceAlertPresenter> presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_
