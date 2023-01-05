// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_view_controller_delegate.h"

@protocol TrackingPriceConsumer;
class ChromeBrowserState;
@class TableViewSwitchItem;

// Mediator for the Tracking Price.
@interface TrackingPriceMediator
    : NSObject <TrackingPriceViewControllerDelegate>

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// View controller.
@property(nonatomic, weak) id<TrackingPriceConsumer> consumer;

// Mobile notification item.
@property(nonatomic, strong) TableViewSwitchItem* mobileNotificationItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_MEDIATOR_H_
