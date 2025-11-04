// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_table_view_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
using ReadingListTableViewItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(ReadingListTableViewItemTest, TextLabels) {
  NSString* titleText = @"Some Title Text";
  NSString* URLText = @"https://www.google.com";

  ReadingListTableViewItem* item =
      [[ReadingListTableViewItem alloc] initWithType:0];
  item.title = titleText;
  CrURL* url = [[CrURL alloc] initWithNSURL:[NSURL URLWithString:URLText]];
  item.entryURL = url.gurl;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(titleText, configuration.title);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url.gurl));
  EXPECT_NSEQ(hostname, configuration.subtitle);
}
