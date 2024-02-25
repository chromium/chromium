// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_cell_delegate.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"

@protocol PriceNotificationsMutator;
@protocol SnackbarCommands;

// View controller that displays PriceNotifications list items in a table view.
@interface PriceNotificationsTableViewController
    : LegacyChromeTableViewController <PriceNotificationsConsumer,
                                       PriceNotificationsTableViewCellDelegate>

// Mutator for Price Tracking related actions e.g price tracking event
// subscription.
@property(nonatomic, weak) id<PriceNotificationsMutator> mutator;

// Handler for displaying snackbar messages on the UI.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Indicates whether this is the user's first time using price tracking.
@property(nonatomic, assign) BOOL hasPreviouslyViewed;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_
