// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
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

  LegacyTableViewCell* cell = [[LegacyTableViewCell alloc] init];
  ASSERT_EQ([item cellClass], cell.class);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(titleText, configuration.title);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              url.gurl));
  EXPECT_NSEQ(hostname, configuration.subtitle);
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

  LegacyTableViewCell* cell = [[LegacyTableViewCell alloc] init];
  ASSERT_EQ([item cellClass], cell.class);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(kTitle, configuration.title);
  NSString* hostname = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              kURL));
  EXPECT_NSEQ(hostname, configuration.subtitle);
  EXPECT_NSEQ(kThirdRowText, configuration.secondSubtitle);
}
