// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/version_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
