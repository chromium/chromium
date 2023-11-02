// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class PriceNotificationsViewController;
@protocol PriceNotificationsNavigationCommands;
@protocol PriceNotificationsViewControllerDelegate;

// Delegate for presentation events related to
// PriceNotificationsViewController.
@protocol PriceNotificationsViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)priceNotificationsViewControllerDidRemove:
    (PriceNotificationsViewController*)controller;

@end

// View controller for Price Notifications setting.
@interface PriceNotificationsViewController
    : SettingsRootTableViewController <PriceNotificationsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<PriceNotificationsViewControllerPresentationDelegate>
        presentationDelegate;

// Delegate for view controller to send responses to model.
@property(nonatomic, weak) id<PriceNotificationsViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_H_
