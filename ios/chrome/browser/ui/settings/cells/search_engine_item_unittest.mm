// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
using SearchEngineItemTest = PlatformTest;
}  // namespace

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(SearchEngineItemTest, BasicProperties) {
  NSString* text = @"Title text";
  NSString* detailText = @"www.google.com";
  GURL URL = net::GURLWithNSURL([NSURL URLWithString:detailText]);

  SearchEngineItem* item = [[SearchEngineItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;
  item.URL = URL;
  item.accessoryType = UITableViewCellAccessoryCheckmark;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  EXPECT_FALSE(URLCell.titleLabel.text);
  EXPECT_FALSE(URLCell.URLLabel.text);
  EXPECT_EQ(item.uniqueIdentifier, URLCell.cellUniqueIdentifier);
  EXPECT_EQ(UITableViewCellAccessoryNone, URLCell.accessoryType);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_NSEQ(text, URLCell.titleLabel.text);
  EXPECT_NSEQ(detailText, URLCell.URLLabel.text);
  EXPECT_EQ(item.uniqueIdentifier, URLCell.cellUniqueIdentifier);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, URLCell.accessoryType);
}

TEST_F(SearchEngineItemTest, isEqual) {
  NSString* text = @"Title text";
  NSString* detailText = @"www.google.com";
  GURL URL = net::GURLWithNSURL([NSURL URLWithString:detailText]);
  NSString* otherText = @"Other Title text";
  NSString* otherDetailText = @"www.notGoogle.com";
  GURL otherURL = net::GURLWithNSURL([NSURL URLWithString:otherDetailText]);

  SearchEngineItem* item = [[SearchEngineItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;
  item.URL = URL;

  SearchEngineItem* sameItem = [[SearchEngineItem alloc] initWithType:0];
  sameItem.text = text;
  sameItem.detailText = detailText;
  sameItem.URL = URL;

  EXPECT_TRUE([item isEqual:sameItem]);

  SearchEngineItem* itemWithDifferentText =
      [[SearchEngineItem alloc] initWithType:0];
  itemWithDifferentText.text = otherText;
  itemWithDifferentText.detailText = item.detailText;
  itemWithDifferentText.URL = item.URL;

  EXPECT_FALSE([item isEqual:itemWithDifferentText]);

  SearchEngineItem* itemWithDifferentDetailText =
      [[SearchEngineItem alloc] initWithType:0];
  itemWithDifferentDetailText.text = item.text;
  itemWithDifferentDetailText.detailText = otherDetailText;
  itemWithDifferentDetailText.URL = item.URL;

  EXPECT_FALSE([item isEqual:itemWithDifferentDetailText]);

  SearchEngineItem* itemWithDifferentURL =
      [[SearchEngineItem alloc] initWithType:0];
  itemWithDifferentURL.text = item.text;
  itemWithDifferentURL.detailText = item.detailText;
  itemWithDifferentURL.URL = otherURL;
}
