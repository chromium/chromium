// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_tab_cell.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using TabStripTabCellTest = PlatformTest;

// Verifies that updating cell selection inside `performWithoutAnimation:` does
// not re-enable UIView animations for the enclosing block.
TEST_F(TabStripTabCellTest, SetSelectedPreservesDisabledAnimations) {
  TabStripTabCell* cell =
      [[TabStripTabCell alloc] initWithFrame:CGRectMake(0, 0, 100, 40)];
  [UIView performWithoutAnimation:^{
    ASSERT_FALSE([UIView areAnimationsEnabled]);
    cell.selected = YES;
    EXPECT_FALSE([UIView areAnimationsEnabled]);
  }];
}
