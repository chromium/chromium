// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

class DefaultPageModeTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[DefaultPageModeTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
  }
};

// Tests that there are 2 items in the Table View.
TEST_F(DefaultPageModeTableViewControllerTest, TestItems) {
  CreateController();
  CheckController();
  CheckTitle(l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_TITLE));

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(2, NumberOfItemsInSection(0));
  CheckTextCellText(@"Mobile", 0, 0);
  CheckTextCellText(@"Desktop", 0, 1);

  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 1);
}

// Tests that the checkmark gets correctly updated when set before the model is
// loaded.
TEST_F(DefaultPageModeTableViewControllerTest, TestCheckmarkAtLoad) {
  // Load the controller manually as this is testing setting the DefaultPageMode
  // before the model is loaded.
  DefaultPageModeTableViewController* controller =
      [[DefaultPageModeTableViewController alloc]
          initWithStyle:UITableViewStyleGrouped];

  [controller setDefaultPageMode:DefaultPageModeDesktop];

  [controller loadModel];
  // Force the tableView to be built.
  ASSERT_TRUE([controller view]);

  UITableViewCellAccessoryType first_accesory =
      [controller.tableViewModel
          itemAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]]
          .accessoryType;
  EXPECT_EQ(UITableViewCellAccessoryNone, first_accesory);
  UITableViewCellAccessoryType second_accesory =
      [controller.tableViewModel
          itemAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]]
          .accessoryType;
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, second_accesory);
}

// Tests that the checkmark gets correctly updated.
TEST_F(DefaultPageModeTableViewControllerTest, TestCheckmark) {
  LegacyChromeTableViewController* chrome_controller = controller();
  DefaultPageModeTableViewController* controller =
      base::apple::ObjCCastStrict<DefaultPageModeTableViewController>(
          chrome_controller);

  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 1);

  [controller setDefaultPageMode:DefaultPageModeMobile];

  CheckAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 1);

  [controller setDefaultPageMode:DefaultPageModeDesktop];

  CheckAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckAccessoryType(UITableViewCellAccessoryCheckmark, 0, 1);
}
