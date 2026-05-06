// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ui/base/device_form_factor.h"
#import "url/gurl.h"

namespace {

enum ItemType : NSInteger {
  kItemTypeSampleOne = kItemTypeEnumZero,
  kItemTypeSampleTwo
};

}  // namespace

class PasswordViewControllerTest : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
    CheckController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    PasswordViewController* view_controller =
        [[PasswordViewController alloc] initWithSearchController:nil];
    [view_controller loadModel];
    return view_controller;
  }

  // Returns the header item at `section`.
  id GetHeaderItem(int section) {
    return [controller().tableViewModel headerForSectionIndex:section];
  }

  // Returns the type of the table view item at `item` in `section`.
  NSInteger GetTableViewItemType(int section, int item) {
    return base::apple::ObjCCastStrict<TableViewItem>(
               GetTableViewItem(section, item))
        .type;
  }
};

// Tests the following:
// 1. "No password items present" message is shown alongside the action items if
// there are no data items.
// 2. "No password items present" message is removed once there are password
// items to be shown in the view.
TEST_F(PasswordViewControllerTest, CheckNoDataItemsMessageRemoved) {
  PasswordViewController* password_view_controller =
      base::apple::ObjCCastStrict<PasswordViewController>(controller());

  ManualFillActionItem* action_item =
      [[ManualFillActionItem alloc] initWithTitle:nil action:nil];
  action_item.type = kItemTypeSampleOne;

  // First, send no data items so that the "no password items to show" message
  // is displayed.
  [password_view_controller presentCredentials:@[]];
  [password_view_controller presentActions:@[ action_item ]];

  // Make sure that the table view content is as expected.
  EXPECT_EQ(NumberOfSections(), 2);
  EXPECT_TRUE(GetHeaderItem(/*section=*/0));
  EXPECT_EQ(NumberOfItemsInSection(0), 0);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1, /*item=*/0),
            kItemTypeSampleOne);

  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithUsername:@"test@example.com"
                                            password:@""
                                            siteName:@""
                                                host:@""
                                                 URL:GURL("https://example.com")
                                  isBackupCredential:NO];

  // Add an password data item.
  ManualFillCredentialItem* password_item = [[ManualFillCredentialItem alloc]
               initWithCredential:credential
                  contentInjector:nil
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:nil
           showAutofillFormButton:NO
          fromAllPasswordsContext:NO
                   credentialType:ManualFillCredentialType::kPassword];
  [password_view_controller presentCredentials:@[ password_item ]];
  // Override the type for the test.
  password_item.type = kItemTypeSampleTwo;

  // Check that the "no password present" message is removed.
  EXPECT_EQ(NumberOfSections(), 2);
  EXPECT_FALSE(GetHeaderItem(/*section=*/0));
  EXPECT_FALSE(GetHeaderItem(/*section=*/1));
  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  EXPECT_EQ(NumberOfItemsInSection(1), 1);
  EXPECT_EQ(GetTableViewItemType(/*section=*/1,
                                 /*item=*/0),
            kItemTypeSampleOne);
  EXPECT_EQ(GetTableViewItemType(/*section=*/0,
                                 /*item=*/0),
            kItemTypeSampleTwo);
}
