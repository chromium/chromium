// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

class LockdownModeViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[LockdownModeViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
  }
};

// Tests that there is a single item in the Table View.
TEST_F(LockdownModeViewControllerTest, TestItems) {
  CreateController();
  CheckController();
  CheckTitle(l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_TITLE));

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));

  CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_LOCKDOWN_MODE_TITLE, 0, 0);
}

// Tests that the switch item gets correctly updated when its value is changed
// before the model is loaded.
TEST_F(LockdownModeViewControllerTest, TestSwitchItemAtLoad) {
  // Load the controller manually as this is testing setting the DefaultPageMode
  // before the model is loaded.
  LockdownModeViewController* controller = [[LockdownModeViewController alloc]
      initWithStyle:UITableViewStyleGrouped];

  [controller setBrowserLockdownModeEnabled:YES];

  [controller loadModel];
  // Force the tableView to be built.
  ASSERT_TRUE([controller view]);

  id switch_item = [controller.tableViewModel
      itemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_TRUE([switch_item respondsToSelector:@selector(isOn)]);
  EXPECT_TRUE([switch_item isOn]);
}

// Tests that the switch item gets correctly updated.
TEST_F(LockdownModeViewControllerTest, TestCheckmark) {
  LegacyChromeTableViewController* chrome_controller = controller();
  LockdownModeViewController* controller =
      base::apple::ObjCCastStrict<LockdownModeViewController>(
          chrome_controller);

  [controller setBrowserLockdownModeEnabled:YES];
  CheckSwitchCellStateAndTextWithId(YES, IDS_IOS_LOCKDOWN_MODE_TITLE, 0, 0);

  [controller setBrowserLockdownModeEnabled:NO];
  CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_LOCKDOWN_MODE_TITLE, 0, 0);
}
