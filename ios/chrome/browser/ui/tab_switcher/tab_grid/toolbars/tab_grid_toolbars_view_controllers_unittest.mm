// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

#import "base/test/metrics/user_action_tester.h"
#import "testing/platform_test.h"

class TabGridToolbarsViewControllersTest : public PlatformTest {
 protected:
  TabGridToolbarsViewControllersTest() {
    top_toolbar_ = [[TabGridTopToolbar alloc] initWithFrame:CGRectZero];
    bottom_toolbar_ = [[TabGridBottomToolbar alloc] initWithFrame:CGRectZero];
  }
  ~TabGridToolbarsViewControllersTest() override {}

  // Checks that both view controllers can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return [top_toolbar_ canPerformAction:NSSelectorFromString(action)
                               withSender:sender] &&
           [bottom_toolbar_ canPerformAction:NSSelectorFromString(action)
                                  withSender:sender];
  }

  // Checks that both view controllers can perform the `action`. The sender is
  // set to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  void ExpectUMA(NSString* action, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [top_toolbar_ performSelector:NSSelectorFromString(action)];
    [bottom_toolbar_ performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 2);
  }

  base::UserActionTester user_action_tester_;
  TabGridTopToolbar* top_toolbar_;
  TabGridBottomToolbar* bottom_toolbar_;
};

// Checks that toolbars implements the following actions.
TEST_F(TabGridToolbarsViewControllersTest, TopToolbarsImplementsActions) {
  // Load the view.
  std::ignore = top_toolbar_;
  [top_toolbar_ keyCommand_closeAll];
  [top_toolbar_ keyCommand_undo];
}

// Checks that toolbars implements the following actions.
TEST_F(TabGridToolbarsViewControllersTest, BottomToolbarsImplementsActions) {
  // Load the view.
  std::ignore = bottom_toolbar_;
  [bottom_toolbar_ keyCommand_closeAll];
  [bottom_toolbar_ keyCommand_undo];
}

// Checks that metrics are correctly reported in toolbars.
TEST_F(TabGridToolbarsViewControllersTest, Metrics) {
  // Load the views.
  std::ignore = top_toolbar_;
  std::ignore = bottom_toolbar_;
  ExpectUMA(@"keyCommand_closeAll", "MobileKeyCommandCloseAll");
  ExpectUMA(@"keyCommand_undo", "MobileKeyCommandUndo");
}
