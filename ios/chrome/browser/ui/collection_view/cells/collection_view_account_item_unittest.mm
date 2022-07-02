// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AccountControlCollectionViewItemTest = PlatformTest;

// Tests that the UIImageView and UILabels are set properly after a call to
// `configureCell:`.
TEST_F(AccountControlCollectionViewItemTest, ImageViewAndTextLabels) {
  CollectionViewAccountItem* item =
      [[CollectionViewAccountItem alloc] initWithType:0];
  UIImage* image = [[UIImage alloc] init];
  NSString* mainText = @"Main text";
  NSString* detailText = @"Detail text";

  item.image = image;
  item.text = mainText;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[CollectionViewAccountCell class]]);

  CollectionViewAccountCell* accountCell = cell;
  EXPECT_FALSE(accountCell.imageView.image);
  EXPECT_FALSE(accountCell.textLabel.text);
  EXPECT_FALSE(accountCell.detailTextLabel.text);

  [item configureCell:cell];
  EXPECT_NSEQ(image, accountCell.imageView.image);
  EXPECT_NSEQ(mainText, accountCell.textLabel.text);
  EXPECT_NSEQ(detailText, accountCell.detailTextLabel.text);
}
