// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_profile_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using AutofillProfileItemTest = PlatformTest;
}

// Tests that the UILabel is set properly after a call to
// `configureCell:` and the image are visible.
TEST_F(AutofillProfileItemTest, ItemProperties) {
  NSString* text = @"Cell text";
  NSString* detailText = @"Detail text";

  AutofillProfileItem* item = [[AutofillProfileItem alloc] initWithType:0];
  item.title = text;
  item.detailText = detailText;
  item.image = [[UIImage alloc] init];

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[AutofillProfileCell class]]);

  AutofillProfileCell* imageCell =
      base::apple::ObjCCastStrict<AutofillProfileCell>(cell);
  EXPECT_FALSE(imageCell.textLabel.text);
  EXPECT_FALSE(imageCell.detailTextLabel.text);
  EXPECT_FALSE(imageCell.imageView.image);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, imageCell.textLabel.text);
  EXPECT_NSEQ(detailText, imageCell.detailTextLabel.text);
  EXPECT_FALSE(imageCell.imageView.isHidden);
}

// Tests that the imageView is not visible if no image is set.
TEST_F(AutofillProfileItemTest, ItemImageViewHidden) {
  NSString* text = @"Cell text";

  AutofillProfileItem* item = [[AutofillProfileItem alloc] initWithType:0];
  item.title = text;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[AutofillProfileCell class]]);

  AutofillProfileCell* imageCell =
      base::apple::ObjCCastStrict<AutofillProfileCell>(cell);
  EXPECT_FALSE(item.image);
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_FALSE(item.image);
  EXPECT_TRUE(imageCell.imageView.isHidden);
}
