// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ReadingListTableViewItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(ReadingListTableViewItemTest, TextLabels) {
  NSString* titleText = @"Some Title Text";
  NSString* URLText = @"https://www.google.com";
  NSString* metadataText = @"Metadata text";

  ReadingListTableViewItem* item =
      [[ReadingListTableViewItem alloc] initWithType:0];
  item.title = titleText;
  CrURL* url = [[CrURL alloc] initWithNSURL:[NSURL URLWithString:URLText]];
  item.entryURL = url.gurl;
  item.distillationSizeText = metadataText;

  TableViewURLCell* URLCell = [[TableViewURLCell alloc] init];
  EXPECT_FALSE(URLCell.titleLabel.text);
  EXPECT_FALSE(URLCell.URLLabel.text);
  EXPECT_FALSE(URLCell.metadataLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_NSEQ(titleText, URLCell.titleLabel.text);
  EXPECT_NSEQ(metadataText, URLCell.metadataLabel.text);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url.gurl));
  EXPECT_NSEQ(hostname, URLCell.URLLabel.text);
}
