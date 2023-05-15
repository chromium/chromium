// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"

@class TrackingPriceViewController;
@protocol TrackingPriceViewControllerDelegate;

// Delegate for presentation events related to
// TrackingPriceViewController.
@protocol TrackingPriceViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)trackingPriceViewControllerDidRemove:
    (TrackingPriceViewController*)controller;

@end

// View controller for tracking price settings.
@interface TrackingPriceViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       TrackingPriceConsumer>

// Presentation delegate.
@property(nonatomic, weak) id<TrackingPriceViewControllerPresentationDelegate>
    presentationDelegate;

// Delegate for view controller to send responses to model.
@property(nonatomic, weak) id<TrackingPriceViewControllerDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_VIEW_CONTROLLER_H_
