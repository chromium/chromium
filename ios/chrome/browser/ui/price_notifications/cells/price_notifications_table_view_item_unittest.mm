// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PriceNotificationsTableViewItemTest = PlatformTest;
// Ensures that the PriceNotificationTableViewCell's track button is visible
// while the menu button is hidden if the cell contains a trackable product.
TEST_F(PriceNotificationsTableViewItemTest,
       EnsuresTrackButtonIsDisplayedForTrackableItems) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];

  cell.tracking = NO;

  EXPECT_EQ(cell.trackButton.hidden, NO);
  EXPECT_EQ(cell.menuButton.hidden, YES);
}

// Ensures that the PriceNotificationTableViewCell's track button is hidden
// while the menu button is visible if the cell contains a tracked product.
TEST_F(PriceNotificationsTableViewItemTest,
       EnsuresTrackButtonIsNotDisplayedForTrackedItems) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];

  cell.tracking = YES;

  EXPECT_EQ(cell.trackButton.hidden, YES);
  EXPECT_EQ(cell.menuButton.hidden, NO);
}

// Ensures that the PriceNotificationTableViewCell's
// PriceNotificationsPriceChipView's background color is gray if only one price
// is given.
TEST_F(PriceNotificationsTableViewItemTest,
       PriceChipViewBackgroundColorIsGrayWithOnePrice) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];

  [cell.priceNotificationsChip setPriceDrop:nil previousPrice:@"$10"];

  EXPECT_EQ([cell.priceNotificationsChip.backgroundColor
                isEqual:[UIColor colorNamed:kGrey100Color]],
            true);
}

// Ensures that the PriceNotificationTableViewCell's
// PriceNotificationsPriceChipView's background color is green if only two
// prices are given.
TEST_F(PriceNotificationsTableViewItemTest,
       PriceChipViewBackgroundColorIsGreenWithPriceDrop) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];

  [cell.priceNotificationsChip setPriceDrop:@"$5" previousPrice:@"$10"];

  EXPECT_EQ([cell.priceNotificationsChip.backgroundColor
                isEqual:[UIColor colorNamed:kGreen100Color]],
            true);
}
