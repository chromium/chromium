// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

class WebInspectorStateTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[WebInspectorStateTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
  }
};

// Tests that there is a single item in the Table View.
TEST_F(WebInspectorStateTableViewControllerTest, TestItems) {
  CreateController();
  CheckController();
  CheckTitle(l10n_util::GetNSString(IDS_IOS_WEB_INSPECTOR_TITLE));

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));

  CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_WEB_INSPECTOR_LABEL, 0, 0);
}

// Tests that the switch item gets correctly updated when its value is changed
// before the model is loaded.
TEST_F(WebInspectorStateTableViewControllerTest, TestSwitchItemAtLoad) {
  // Load the controller manually as this is testing setting the DefaultPageMode
  // before the model is loaded.
  WebInspectorStateTableViewController* controller =
      [[WebInspectorStateTableViewController alloc]
          initWithStyle:UITableViewStyleGrouped];

  [controller setWebInspectorEnabled:YES];

  [controller loadModel];
  // Force the tableView to be built.
  ASSERT_TRUE([controller view]);

  id switch_item = [controller.tableViewModel
      itemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_TRUE([switch_item respondsToSelector:@selector(isOn)]);
  EXPECT_TRUE([switch_item isOn]);
}

// Tests that the switch item gets correctly updated.
TEST_F(WebInspectorStateTableViewControllerTest, TestCheckmark) {
  LegacyChromeTableViewController* chrome_controller = controller();
  WebInspectorStateTableViewController* controller =
      base::apple::ObjCCastStrict<WebInspectorStateTableViewController>(
          chrome_controller);

  [controller setWebInspectorEnabled:YES];
  CheckSwitchCellStateAndTextWithId(YES, IDS_IOS_WEB_INSPECTOR_LABEL, 0, 0);

  [controller setWebInspectorEnabled:NO];
  CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_WEB_INSPECTOR_LABEL, 0, 0);
}
