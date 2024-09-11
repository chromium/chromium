// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class TabGridToolbarsViewControllersTest : public PlatformTest {
 protected:
  TabGridToolbarsViewControllersTest() {
    top_toolbar_ = [[TabGridTopToolbar alloc] initWithFrame:CGRectZero];
    bottom_toolbar_ = [[TabGridBottomToolbar alloc] initWithFrame:CGRectZero];
  }
  ~TabGridToolbarsViewControllersTest() override {}

  // Checks if the given `toolbar` objet can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender, UIResponder* toolbar) {
    return [toolbar canPerformAction:NSSelectorFromString(action)
                          withSender:sender];
  }

  // Checks that both view controllers can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return CanPerform(action, sender, top_toolbar_) &&
           CanPerform(action, sender, bottom_toolbar_);
  }

  // Checks that both view controllers can perform the `action`. The sender is
  // set to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  // Checks is the given `toolbar` object register the correct `user_action`
  // after performing the `action`.
  void ExpectUMA(NSString* action,
                 const std::string& user_action,
                 NSObject* toolbar) {
    user_action_tester_.ResetCounts();
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [toolbar performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1)
        << action << " Failed.";
  }

  // Checks both toolbars user action registration.
  void ExpectUMA(NSString* action, const std::string& user_action) {
    ExpectUMA(action, user_action, top_toolbar_);
    ExpectUMA(action, user_action, bottom_toolbar_);
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
  [top_toolbar_ keyCommand_find];
  [top_toolbar_ keyCommand_close];
}

// Checks that toolbars implements the following actions.
TEST_F(TabGridToolbarsViewControllersTest, BottomToolbarsImplementsActions) {
  // Load the view.
  std::ignore = bottom_toolbar_;
  [bottom_toolbar_ keyCommand_closeAll];
  [bottom_toolbar_ keyCommand_undo];
  [bottom_toolbar_ keyCommand_close];
}

// Checks that metrics are correctly reported in toolbars.
TEST_F(TabGridToolbarsViewControllersTest, Metrics) {
  // Load the views.
  std::ignore = top_toolbar_;
  std::ignore = bottom_toolbar_;
  ExpectUMA(@"keyCommand_closeAll", "MobileKeyCommandCloseAll");
  ExpectUMA(@"keyCommand_undo", "MobileKeyCommandUndo");
  ExpectUMA(@"keyCommand_close", "MobileKeyCommandClose");

  // Check only top toolbar.
  ExpectUMA(@"keyCommand_find", "MobileKeyCommandSearchTabs", top_toolbar_);
}

// Checks that the ESC keyboard shortcut is always possible.
TEST_F(TabGridToolbarsViewControllersTest, CanPerform_Close) {
  EXPECT_TRUE(CanPerform(@"keyCommand_close"));
}

// This test ensure 2 things:
// * the key command find is available top toolbar only,
// * the key command associated title is correct.
TEST_F(TabGridToolbarsViewControllersTest, ValidateCommand_find) {
  EXPECT_FALSE(CanPerform(@"keyCommand_find", nil, bottom_toolbar_));
  EXPECT_TRUE(CanPerform(@"keyCommand_find", nil, top_toolbar_));
  id findTarget = [top_toolbar_ targetForAction:@selector(keyCommand_find)
                                     withSender:nil];
  EXPECT_EQ(findTarget, top_toolbar_);

  // Ensures that the title is correct.
  for (UIKeyCommand* command in top_toolbar_.keyCommands) {
    [top_toolbar_ validateCommand:command];
    if (command.action == @selector(keyCommand_find)) {
      EXPECT_NSEQ(
          command.discoverabilityTitle,
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_SEARCH_TABS));
    }
  }
}
