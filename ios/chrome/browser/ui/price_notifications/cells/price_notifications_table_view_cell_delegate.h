// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_CELL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_CELL_DELEGATE_H_

#import <Foundation/Foundation.h>

@class PriceNotificationsTableViewCell;

// Delegate object that manipulates a product's subscription state.
@protocol PriceNotificationsTableViewCellDelegate

// Initiates the user's subscription to the product's price tracking events.
- (void)trackItemForCell:(PriceNotificationsTableViewCell*)cell;

// Initiates the user's unsubscription to the product's price tracking events.
- (void)stopTrackingItemForCell:(PriceNotificationsTableViewCell*)cell;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_CELL_DELEGATE_H_
