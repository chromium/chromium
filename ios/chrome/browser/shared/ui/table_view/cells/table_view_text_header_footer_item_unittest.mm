// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
using TableViewTextHeaderFooterItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to
// `configureHeaderFooterView:`.
TEST_F(TableViewTextHeaderFooterItemTest, HeaderFooterTextLabels) {
  NSString* text = @"HeaderFooter text";

  TableViewTextHeaderFooterItem* item =
      [[TableViewTextHeaderFooterItem alloc] initWithType:0];
  item.text = text;

  id headerFooter = [[[item cellClass] alloc] init];
  ASSERT_TRUE(
      [headerFooter isMemberOfClass:[TableViewTextHeaderFooterView class]]);

  TableViewTextHeaderFooterView* textHeaderFooter =
      base::apple::ObjCCastStrict<TableViewTextHeaderFooterView>(headerFooter);
  EXPECT_FALSE(textHeaderFooter.textLabel.text);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureHeaderFooterView:textHeaderFooter withStyler:styler];
  EXPECT_NSEQ(text, textHeaderFooter.textLabel.text);
}
