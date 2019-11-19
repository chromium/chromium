// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using TableViewURLItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to |configureCell:|.
TEST_F(TableViewURLItemTest, TextLabels) {
  NSString* titleText = @"Title text";
  NSString* host = @"www.google.com";
  NSString* URLText = [NSString stringWithFormat:@"https://%@", host];
  NSString* metadataText = @"Metadata text";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = titleText;
  item.URL = net::GURLWithNSURL([NSURL URLWithString:URLText]);
  item.metadata = metadataText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
  EXPECT_FALSE(URLCell.titleLabel.text);
  EXPECT_FALSE(URLCell.URLLabel.text);
  EXPECT_FALSE(URLCell.metadataLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_NSEQ(titleText, URLCell.titleLabel.text);
  EXPECT_NSEQ(host, URLCell.URLLabel.text);
  EXPECT_NSEQ(metadataText, URLCell.metadataLabel.text);
}

TEST_F(TableViewURLItemTest, MetadataLabelIsHiddenWhenEmpty) {
  NSString* metadataText = nil;

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.metadata = metadataText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
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

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
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
      stringWithFormat:@"%s %@ %@", kURL.host().c_str(),
                       kSupplementalURLTextDelimiter, kSupplementalURLText];

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.title = kTitle;
  item.URL = kURL;
  item.supplementalURLText = kSupplementalURLText;
  item.supplementalURLTextDelimiter = kSupplementalURLTextDelimiter;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  EXPECT_NSEQ(kExpectedURLLabelText,
              base::mac::ObjCCast<TableViewURLCell>(cell).URLLabel.text);
}

// Tests that when there is no title, the URL is used as the title and the
// supplemental URL text is used in the URL label.
TEST_F(TableViewURLItemTest, SupplementalURLTextWithNoTitle) {
  const GURL kURL("https://www.google.com");
  NSString* const kSupplementalURLText = @"supplement";

  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:0];
  item.URL = kURL;
  item.supplementalURLText = kSupplementalURLText;

  id cell = [[[item cellClass] alloc] init];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);
  TableViewURLCell* url_cell = base::mac::ObjCCast<TableViewURLCell>(cell);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kURL.host()), url_cell.titleLabel.text);
  EXPECT_NSEQ(kSupplementalURLText, url_cell.URLLabel.text);
}
