// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_mutator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller+testing.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class AutofillAIEntityEditTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[AutofillAIEntityEditTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
  }

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    mock_delegate_ = OCMProtocolMock(
        @protocol(AutofillAIEntityEditTableViewControllerDelegate));
    mock_mutator_ = OCMProtocolMock(@protocol(AutofillAIEntityEditMutator));

    AutofillAIEntityEditTableViewController* view_controller =
        base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
            controller());
    view_controller.delegate = mock_delegate_;
    view_controller.mutator = mock_mutator_;
  }

  id mock_delegate_;
  id mock_mutator_;
};

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(1, NumberOfSections());
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestLoadModel) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  AutofillAIEntityEditItem* item =
      [[AutofillAIEntityEditItem alloc] initWithType:kItemTypeEnumZero];
  [view_controller setEditItems:@[ item ]];

  CheckController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestEditButtonPressedSave) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  [view_controller setEditing:YES animated:NO];
  EXPECT_TRUE(view_controller.editing);

  OCMExpect([mock_mutator_ saveEntityInstance]);

  [view_controller editButtonPressed];

  EXPECT_FALSE(view_controller.editing);
  [mock_mutator_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestSelectCountryItem) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  AutofillAIEntityCountryItem* item =
      [[AutofillAIEntityCountryItem alloc] initWithType:kItemTypeEnumZero];
  [view_controller setEditItems:@[ item ]];

  CheckController();
  [view_controller setEditing:YES animated:NO];

  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  OCMExpect([mock_delegate_ didTapCountryItem:item]);

  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:indexPath];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestDidTapSaveNewEntity) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  view_controller.startInEditMode = YES;
  [view_controller loadViewIfNeeded];

  // Expect the mutator to save and the delegate to close.
  OCMExpect([mock_mutator_ saveEntityInstance]);
  OCMExpect([mock_delegate_ didTapCloseButton:view_controller]);

  // Trigger the save action.
  [view_controller didTapSaveNewEntity];

  [mock_mutator_ verify];
  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestDidTapCancel) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  view_controller.startInEditMode = YES;
  [view_controller loadViewIfNeeded];

  // Expect the delegate to close the view controller.
  OCMExpect([mock_delegate_ didTapCloseButton:view_controller]);

  // Trigger the cancel action.
  [view_controller didTapCancel];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestStartInEditModeHidesDoneButton) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  view_controller.startInEditMode = YES;
  [view_controller loadViewIfNeeded];

  // Verify that the top right Done button is hidden and edit button isn't
  // shown.
  EXPECT_TRUE(view_controller.shouldHideDoneButton);
  EXPECT_FALSE([view_controller shouldShowEditDoneButton]);
  EXPECT_FALSE([view_controller shouldShowEditButton]);
}

}  // namespace
