// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

@class PriceNotificationsImageContainerView;
@class PriceNotificationsPriceChipView;

// A table view item used to represent a `PriceNotificationsListItem`.
@interface PriceNotificationsTableViewItem : TableViewItem

// Title of the trackable item.
@property(nonatomic, copy) NSString* title;
// URL of the trackable item.
@property(nonatomic, copy) NSString* entryURL;
// The price at which the user began tracking the item.
@property(nonatomic, copy) NSString* previousPrice;
// The current discounted price of the item.
@property(nonatomic, copy) NSString* currentPrice;
// The item's image.
@property(nonatomic, strong) UIImage* productImage;
// The status of whether the user is tracking the item.
@property(nonatomic, assign) BOOL tracking;

@end

// PriceNotificationsTableViewCell implements an TableViewCell subclass
// containing a leading image, two text labels (title and url) and a custom
// UIView containing the item's price laid out vertically, and either a UIButton
// to initiate tracking the item or a menu button to manage the item.
@interface PriceNotificationsTableViewCell : TableViewCell

// Sets the item's image.
- (void)setImage:(UIImage*)productImage;

// The cell title.
@property(nonatomic, strong) UILabel* titleLabel;
// The host URL associated with this cell.
@property(nonatomic, strong) UILabel* URLLabel;
// The custom UIView that displays the item's current and previous prices.
@property(nonatomic, strong)
    PriceNotificationsPriceChipView* priceNotificationsChip;
// The status of whether the user is tracking the item.
@property(nonatomic, assign) BOOL tracking;
// The button that starts the price tracking process.
@property(nonatomic, strong) UIButton* trackButton;
// The button that displays user controlled settings for the item.
@property(nonatomic, strong) UIButton* menuButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_TABLE_VIEW_ITEM_H_
