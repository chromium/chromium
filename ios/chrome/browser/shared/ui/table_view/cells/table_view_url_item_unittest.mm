// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
using TableViewURLItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(TableViewURLItemTest, TextLabels) {
  NSString* titleText = @"Title text";
  NSString* host = @"www.google.com";
  NSString* URLText = [NSString stringWithFormat:@"https://%@", host];

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = titleText;
  CrURL* url = [[CrURL alloc] initWithNSURL:[NSURL URLWithString:URLText]];
  item.URL = url;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  EXPECT_FALSE(URLCell.titleLabel.text);
  EXPECT_FALSE(URLCell.URLLabel.text);
  EXPECT_FALSE(URLCell.metadataLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_NSEQ(titleText, URLCell.titleLabel.text);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              url.gurl));
  EXPECT_NSEQ(hostname, URLCell.URLLabel.text);
}

// Tests that the third row text is shown when the other two rows are shown.
TEST_F(TableViewURLItemTest, ThirdRowText) {
  NSString* const kTitle = @"Title";
  const GURL kURL("https://www.google.com");
  NSString* const kThirdRowText = @"third-row";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.URL = [[CrURL alloc] initWithGURL:kURL];
  item.thirdRowText = kThirdRowText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  EXPECT_NSEQ(kThirdRowText,
              base::apple::ObjCCast<TableViewURLCell>(cell).thirdRowLabel.text);
  EXPECT_FALSE(
      base::apple::ObjCCast<TableViewURLCell>(cell).thirdRowLabel.hidden);
}

// Tests that the third row text is not shown when the second row is not shown.
TEST_F(TableViewURLItemTest, ThirdRowTextNotShown) {
  NSString* const kTitle = @"Title";
  NSString* const kThirdRowText = @"third-row";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.thirdRowText = kThirdRowText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  EXPECT_TRUE(
      base::apple::ObjCCast<TableViewURLCell>(cell).thirdRowLabel.hidden);
}

// Tests that the third row text is included in the accessibility label.
TEST_F(TableViewURLItemTest, ThirdRowTextAccessibilityLabel) {
  NSString* const kTitle = @"Title";
  NSString* const kDetail = @"Detail";
  NSString* const kThirdRowText = @"third-row";
  NSString* const kExpectedAccessibilityText = @"Title, Detail, third-row";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.detailText = kDetail;
  item.thirdRowText = kThirdRowText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  UITableViewCell* tableViewCell = base::apple::ObjCCast<UITableViewCell>(cell);
  tableViewCell.accessibilityLabel = nil;
  EXPECT_NSEQ(kExpectedAccessibilityText, tableViewCell.accessibilityLabel);
}
