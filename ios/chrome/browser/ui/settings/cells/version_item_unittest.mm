// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/version_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using VersionItemTest = PlatformTest;

TEST_F(VersionItemTest, TextLabelGetsText) {
  VersionItem* item = [[VersionItem alloc] initWithType:0];
  VersionFooter* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[VersionFooter class]]);

  item.text = @"Foo";
  [item configureHeaderFooterView:cell
                       withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(@"Foo", cell.textLabel.text);
}

}  // namespace
