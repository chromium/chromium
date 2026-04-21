// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"

#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_mutator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller+testing.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

@interface FakeMutator : NSObject <AutofillAIEntityEditMutator>
@property(nonatomic, assign) autofill::DenseSet<autofill::AttributeType>
    missingFields;
@end

@implementation FakeMutator
- (void)saveEntityInstance {
}
- (void)didChangeDate:(NSDate*)date
              forItem:(AutofillAIEntityEditDateItem*)item {
}
- (autofill::DenseSet<autofill::AttributeType>)getMissingRequiredFieldsFor:
    (const autofill::DenseSet<autofill::AttributeType>&)presentAttributes {
  return _missingFields;
}
- (void)requestEditingWithCompletion:
    (void (^)(ReauthenticationResult result))completion {
}
@end

namespace {

class AutofillAIEntityEditTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    AutofillAIEntityEditTableViewController* controller =
        [[AutofillAIEntityEditTableViewController alloc]
            initWithStyle:UITableViewStyleGrouped];

    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    std::string_view test_name = test_info->name();

    if (test_name == "TestDidTapSaveNewEntity" ||
        test_name == "TestDidTapCancel" ||
        test_name == "TestStartInEditModeHidesDoneButton" ||
        test_name == "TestDidFinishSavingWithLocalFallbackTrue" ||
        test_name == "TestDidFinishSavingWithLocalFallbackFalse" ||
        test_name == "TestSaveButtonState") {
      controller.mode = AutofillAIEntityEditMode::kCreate;
    }

    return controller;
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

  // Expect the mutator to save.
  OCMExpect([mock_mutator_ saveEntityInstance]);

  // Trigger the save action.
  [view_controller didTapSaveNewEntity];

  [mock_mutator_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestEditButtonPressedForWalletItem) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  // Mark the item as a Server Wallet item.
  [view_controller setIsServerWalletItem:YES];

  // Expect the delegate to handle the external Wallet edit flow.
  OCMExpect([mock_delegate_ didTapEditInWalletButton:view_controller]);

  // Trigger the edit action.
  [view_controller editButtonPressed];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestDidTapCancel) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  // Expect the delegate to close the view controller.
  OCMExpect([mock_delegate_ dismissViewController:view_controller]);

  // Trigger the cancel action.
  [view_controller didTapCancel];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestStartInEditModeHidesDoneButton) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  // Verify that the top right Done button is hidden and edit button isn't
  // shown.
  EXPECT_TRUE(view_controller.shouldHideDoneButton);
  EXPECT_FALSE([view_controller shouldShowEditDoneButton]);
  EXPECT_FALSE([view_controller shouldShowEditButton]);
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestFooterForLocalItem) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  [view_controller setIsServerWalletItem:NO];
  [view_controller loadModel];

  TableViewLinkHeaderFooterItem* footer =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
          [view_controller.tableViewModel footerForSectionIndex:0]);

  EXPECT_TRUE(footer);
  EXPECT_NSEQ(footer.text,
              l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SAVED_LOCALLY_FOOTER));
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestFooterForWalletItemWithEmail) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  NSString* test_email = @"test@gmail.com";
  [view_controller setIsServerWalletItem:YES];
  [view_controller setUserEmail:test_email];
  [view_controller loadModel];

  TableViewLinkHeaderFooterItem* footer =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
          [view_controller.tableViewModel footerForSectionIndex:0]);

  EXPECT_TRUE(footer);
  EXPECT_EQ(1U, footer.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), footer.urls[0].gurl);
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestDidFinishSavingWithLocalFallbackTrue) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  // Expect the delegate to be notified of the fallback.
  OCMExpect([mock_delegate_ showLocalSaveFallbackAlert]);

  // Trigger the fallback scenario.
  [view_controller didFinishSavingWithLocalFallback:YES];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestDidFinishSavingWithLocalFallbackFalse) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  // Expect the delegate to process a standard dismissal.
  OCMExpect([mock_delegate_ dismissViewController:view_controller]);

  // Trigger the successful save scenario.
  [view_controller didFinishSavingWithLocalFallback:NO];

  [mock_delegate_ verify];
}

TEST_F(AutofillAIEntityEditTableViewControllerTest, TestSaveButtonState) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  FakeMutator* fake_mutator = [[FakeMutator alloc] init];
  autofill::DenseSet<autofill::AttributeType> missing_fields;
  missing_fields.insert(
      autofill::AttributeType(autofill::AttributeTypeName::kPassportName));
  fake_mutator.missingFields = missing_fields;
  view_controller.mutator = fake_mutator;

  [view_controller tableViewItemDidChange:nil];

  EXPECT_FALSE(view_controller.saveButton.enabled);

  missing_fields.clear();
  fake_mutator.missingFields = missing_fields;

  [view_controller tableViewItemDidChange:nil];

  EXPECT_TRUE(view_controller.saveButton.enabled);
}

TEST_F(AutofillAIEntityEditTableViewControllerTest,
       TestValidationPreventedUntilSetEditItemsCompleted) {
  AutofillAIEntityEditTableViewController* view_controller =
      base::apple::ObjCCastStrict<AutofillAIEntityEditTableViewController>(
          controller());

  FakeMutator* fake_mutator = [[FakeMutator alloc] init];
  autofill::DenseSet<autofill::AttributeType> missing_fields;
  missing_fields.insert(
      autofill::AttributeType(autofill::AttributeTypeName::kPassportName));
  fake_mutator.missingFields = missing_fields;
  view_controller.mutator = fake_mutator;

  AutofillAIEntityEditItem* item =
      [[AutofillAIEntityEditItem alloc] initWithType:kItemTypeEnumZero];
  item.attributeType = autofill::AttributeTypeName::kPassportName;

  EXPECT_TRUE(item.hasValidValueStatus);

  [view_controller setEditItems:@[ item ]];

  EXPECT_TRUE(item.hasValidValueStatus);

  [view_controller tableViewItemDidChange:item];

  EXPECT_FALSE(item.hasValidValueStatus);
}

}  // namespace
