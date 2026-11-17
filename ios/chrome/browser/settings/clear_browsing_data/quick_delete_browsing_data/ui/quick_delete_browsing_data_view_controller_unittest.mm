// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/clear_browsing_data/quick_delete_browsing_data/ui/quick_delete_browsing_data_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class QuickDeleteBrowsingDataViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[QuickDeleteBrowsingDataViewController alloc] init];
    scoped_window_.Get().rootViewController = view_controller_;
    [view_controller_ loadViewIfNeeded];
    [view_controller_.tableView layoutIfNeeded];
  }

  UITableViewCell* GetCellForRow(NSInteger row) {
    [view_controller_.tableView layoutIfNeeded];
    NSIndexPath* index_path = [NSIndexPath indexPathForRow:row inSection:0];
    return [view_controller_.tableView cellForRowAtIndexPath:index_path];
  }

  void ExpectCellState(NSInteger row, bool enabled, bool selected) {
    SCOPED_TRACE(testing::Message() << "row=" << row);
    UITableViewCell* cell = GetCellForRow(row);
    ASSERT_TRUE(cell);
    EXPECT_EQ(static_cast<bool>(cell.userInteractionEnabled), enabled);
    EXPECT_DOUBLE_EQ(cell.contentView.alpha, enabled ? 1.0 : 0.5);
    EXPECT_EQ(cell.accessoryType, (enabled && selected)
                                      ? UITableViewCellAccessoryCheckmark
                                      : UITableViewCellAccessoryNone);
    EXPECT_EQ(cell.accessibilityTraits & UIAccessibilityTraitNotEnabled,
              enabled ? 0U : UIAccessibilityTraitNotEnabled);
    EXPECT_EQ(cell.accessibilityTraits & UIAccessibilityTraitSelected,
              (enabled && selected) ? UIAccessibilityTraitSelected : 0U);
  }

  ScopedKeyWindow scoped_window_;
  QuickDeleteBrowsingDataViewController* view_controller_;
};

// Tests that toggling `setHistoryDeletionEnabled:` from NO to YES on the same
// view controller instance updates and restores the History cell's interaction,
// alpha, accessory, and accessibility traits without leaking disabled state to
// reused cells.
TEST_F(QuickDeleteBrowsingDataViewControllerTest,
       HistoryDeletionEnabledTogglesCellState) {
  [view_controller_ setHistorySelection:YES];
  [view_controller_ setTabsSelection:YES];

  ExpectCellState(/*row=*/0, /*enabled=*/true, /*selected=*/true);

  // Disable history deletion and verify the History cell becomes disabled and
  // deselected while reloading another row does not inherit the disabled state.
  [view_controller_ setHistoryDeletionEnabled:NO];
  [view_controller_ setTabsSummary:@"2 tabs"];

  ExpectCellState(/*row=*/0, /*enabled=*/false, /*selected=*/false);
  ExpectCellState(/*row=*/1, /*enabled=*/true, /*selected=*/true);

  // Re-enable history deletion on the same view controller and verify the
  // History cell restores its enabled and selected state.
  [view_controller_ setHistoryDeletionEnabled:YES];
  [view_controller_ setHistorySelection:YES];

  ExpectCellState(/*row=*/0, /*enabled=*/true, /*selected=*/true);
}

}  // namespace
