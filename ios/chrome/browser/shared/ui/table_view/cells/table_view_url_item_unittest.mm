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
  NSString* metadataText = @"Metadata text";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = titleText;
  CrURL* url = [[CrURL alloc] initWithNSURL:[NSURL URLWithString:URLText]];
  item.URL = url;
  item.metadata = metadataText;

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
  EXPECT_NSEQ(metadataText, URLCell.metadataLabel.text);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              url.gurl));
  EXPECT_NSEQ(hostname, URLCell.URLLabel.text);
}

TEST_F(TableViewURLItemTest, MetadataLabelIsHiddenWhenEmpty) {
  NSString* metadataText = nil;

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.metadata = metadataText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_TRUE(URLCell.metadataLabel.hidden);
}

TEST_F(TableViewURLItemTest, MetadataLabelIsVisibleWhenNonEmpty) {
  NSString* metadataText = @"Metadata text";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.metadata = metadataText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_FALSE(URLCell.metadataLabel.hidden);
}

// Tests that the suppelemental URL text is appended to the hostname when there
// is a title.
TEST_F(TableViewURLItemTest, SupplementalURLTextWithTitle) {
  NSString* const kTitle = @"Title";
  const GURL kURL("https://www.google.com");
  NSString* const kSupplementalURLText = @"supplement";
  NSString* const kSupplementalURLTextDelimiter = @"x";
  NSString* const kExpectedURLLabelText = [NSString
      stringWithFormat:
          @"%@ %@ %@",
          base::SysUTF16ToNSString(
              url_formatter::
                  FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                      kURL)),
          kSupplementalURLTextDelimiter, kSupplementalURLText];

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.URL = [[CrURL alloc] initWithGURL:kURL];
  item.supplementalURLText = kSupplementalURLText;
  item.supplementalURLTextDelimiter = kSupplementalURLTextDelimiter;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  EXPECT_NSEQ(kExpectedURLLabelText,
              base::apple::ObjCCast<TableViewURLCell>(cell).URLLabel.text);
}

// Tests that when there is no title, the URL is used as the title and the
// supplemental URL text is used in the URL label.
TEST_F(TableViewURLItemTest, SupplementalURLTextWithNoTitle) {
  const GURL kURL("https://www.google.com");
  NSString* const kSupplementalURLText = @"supplement";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.URL = [[CrURL alloc] initWithGURL:kURL];
  item.supplementalURLText = kSupplementalURLText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  TableViewURLCell* url_cell = base::apple::ObjCCast<TableViewURLCell>(cell);
  EXPECT_NSEQ(
      base::SysUTF16ToNSString(
          url_formatter::
              FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                  kURL)),
      url_cell.titleLabel.text);
  EXPECT_NSEQ(kSupplementalURLText, url_cell.URLLabel.text);
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

// Tests that the third row text is shown in chosen color.
TEST_F(TableViewURLItemTest, ThirdRowTextColor) {
  NSString* const kTitle = @"Title";
  const GURL kURL("https://www.google.com");
  NSString* const kThirdRowText = @"third-row";
  UIColor* const kExpectedColor = UIColor.greenColor;

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.URL = [[CrURL alloc] initWithGURL:kURL];
  item.thirdRowText = kThirdRowText;
  item.thirdRowTextColor = kExpectedColor;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  EXPECT_NSEQ(
      kExpectedColor,
      base::apple::ObjCCast<TableViewURLCell>(cell).thirdRowLabel.textColor);
}

// Tests that the third row text is included in the accessibility label.
TEST_F(TableViewURLItemTest, ThirdRowTextAccessibilityLabel) {
  NSString* const kTitle = @"Title";
  NSString* const kDetail = @"Detail";
  NSString* const kThirdRowText = @"third-row";
  NSString* const kMetadataText = @"Metadata";
  NSString* const kExpectedAccessibilityText =
      @"Title, Detail, third-row, Metadata";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.detailText = kDetail;
  item.thirdRowText = kThirdRowText;
  item.metadata = kMetadataText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  UITableViewCell* tableViewCell = base::apple::ObjCCast<UITableViewCell>(cell);
  tableViewCell.accessibilityLabel = nil;
  EXPECT_NSEQ(kExpectedAccessibilityText, tableViewCell.accessibilityLabel);
}
