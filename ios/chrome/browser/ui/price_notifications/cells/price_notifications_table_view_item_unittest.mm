// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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

// Ensures that the PriceNotificationsTableViewCell's Menu Button can be tapped
// if the users taps anywhere within a 44pt x 44pt area around the menu button.
TEST_F(PriceNotificationsTableViewItemTest,
       MenuButtonIsTappedWithPointsWithinBoundary) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];
  CGPoint valid_points[] = {CGPointMake(0, 0),    CGPointMake(10, 5),
                            CGPointMake(20, -10), CGPointMake(22, 0),
                            CGPointMake(-22, 0),  CGPointMake(0, 22),
                            CGPointMake(0, -22),  CGPoint(22, -22)};

  for (CGPoint point : valid_points) {
    EXPECT_TRUE([cell.menuButton pointInside:point withEvent:nil]);
  }
}

// Ensures that the PriceNotificationsTableViewCell's Menu Button is not
// activated if the users taps anywhere outside of a 44pt x 44pt area around the
// menu button.
TEST_F(PriceNotificationsTableViewItemTest,
       MenuButtonIsTappedWithPointsOutsideBoundary) {
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];
  CGPoint valid_points[] = {CGPointMake(23, 0),   CGPointMake(-24, 0),
                            CGPointMake(0, 23),   CGPointMake(0, -24),
                            CGPointMake(23, -23), CGPointMake(100, 0)};

  for (CGPoint point : valid_points) {
    EXPECT_FALSE([cell.menuButton pointInside:point withEvent:nil]);
  }
}

// Ensures that a PriceNotificationTableViewItem can properly configure a
// PriceNotificaitonTableViewCell.
TEST_F(PriceNotificationsTableViewItemTest,
       PriceNotificationTableViewCellIsConfiguredByItem) {
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:0];
  item.title = @"Test Item";
  item.entryURL = GURL("https://example.com/product/x");
  item.tracking = NO;
  item.loading = YES;
  item.currentPrice = @"$1.00";
  item.previousPrice = nil;
  item.productImage = UIGraphicsGetImageFromCurrentImageContext();
  PriceNotificationsTableViewCell* cell =
      [[PriceNotificationsTableViewCell alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:nil];

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  EXPECT_NSEQ(item.title, cell.titleLabel.text);
  EXPECT_NSEQ(cell.URLLabel.text, @"example.com");
  EXPECT_TRUE(cell.isLoading);

  [cell prepareForReuse];
  EXPECT_FALSE(cell.isLoading);
}
