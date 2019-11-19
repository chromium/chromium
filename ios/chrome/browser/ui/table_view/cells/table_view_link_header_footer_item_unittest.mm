// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using TableViewLinkHeaderFooterItemTest = PlatformTest;
}  // namespace

TEST_F(TableViewLinkHeaderFooterItemTest, LinkedLabel) {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:0];
  item.text = @"HeaderFooter text with BEGIN_LINKlinkEND_LINK";
  item.linkURL = GURL("https://chromium.org");

  id headerFooter = [[[item cellClass] alloc] init];
  ASSERT_TRUE(
      [headerFooter isMemberOfClass:[TableViewLinkHeaderFooterView class]]);

  TableViewLinkHeaderFooterView* linkHeaderFooterView =
      base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(headerFooter);
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureHeaderFooterView:linkHeaderFooterView withStyler:styler];
  EXPECT_NSEQ(@"HeaderFooter text with link",
              linkHeaderFooterView.accessibilityLabel);
  // Valid item.linkURL adds "Link" trait to the view's accessibility traits.
  EXPECT_EQ(UIAccessibilityTraitLink, linkHeaderFooterView.accessibilityTraits);
}

TEST_F(TableViewLinkHeaderFooterItemTest, UnlinkedLabel) {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:0];
  item.text = @"HeaderFooter text without link";

  id headerFooter = [[[item cellClass] alloc] init];
  ASSERT_TRUE(
      [headerFooter isMemberOfClass:[TableViewLinkHeaderFooterView class]]);

  TableViewLinkHeaderFooterView* linkHeaderFooterView =
      base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(headerFooter);
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureHeaderFooterView:linkHeaderFooterView withStyler:styler];
  EXPECT_NSEQ(@"HeaderFooter text without link",
              linkHeaderFooterView.accessibilityLabel);
  // Without setting item.linkURL, view's accessibility traits should not
  // contain "Link" trait.
  EXPECT_NE(UIAccessibilityTraitLink, linkHeaderFooterView.accessibilityTraits);
}
