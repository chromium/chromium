// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using TableViewHeaderFooterItemTest = PlatformTest;

TEST_F(TableViewHeaderFooterItemTest,
       ConfigureHeaderFooterPortsAccessibilityProperties) {
  TableViewHeaderFooterItem* item =
      [[TableViewHeaderFooterItem alloc] initWithType:0];
  item.accessibilityIdentifier = @"test_identifier";
  item.accessibilityTraits = UIAccessibilityTraitButton;
  UITableViewHeaderFooterView* headerFooterView =
      [[[item cellClass] alloc] init];
  EXPECT_TRUE(
      [headerFooterView isMemberOfClass:[UITableViewHeaderFooterView class]]);
  EXPECT_EQ(UIAccessibilityTraitNone, [headerFooterView accessibilityTraits]);
  EXPECT_FALSE([headerFooterView accessibilityIdentifier]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureHeaderFooterView:headerFooterView withStyler:styler];
  EXPECT_EQ(UIAccessibilityTraitButton, [headerFooterView accessibilityTraits]);
  EXPECT_NSEQ(@"test_identifier", [headerFooterView accessibilityIdentifier]);
}

}  // namespace
