// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using AccountControlTableViewItemTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly after a call to
// `configureCell:`.
TEST_F(AccountControlTableViewItemTest, ImageViewAndTextLabels) {
  TableViewAccountItem* item = [[TableViewAccountItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  item.image = image;
  item.text = mainText;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewAccountCell class]]);

  TableViewAccountCell* accountCell = cell;
  EXPECT_FALSE(accountCell.imageView.image);
  EXPECT_FALSE(accountCell.textLabel.text);
  EXPECT_FALSE(accountCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(image, accountCell.imageView.image);
  EXPECT_NSEQ(mainText, accountCell.textLabel.text);
  EXPECT_NSEQ(detailText, accountCell.detailTextLabel.text);
}
