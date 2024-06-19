// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const char16_t kTestSyncingEmail[] = u"test@email.com";

class LegacyInfobarEditAddressProfileTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    delegate_modal_mock_ = OCMProtocolMock(@protocol(InfobarModalDelegate));
    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();
    profile_ = std::make_unique<autofill::AutofillProfile>(
        autofill::test::GetFullProfile2());
    autofill_profile_edit_mediator_ = [[AutofillProfileEditMediator alloc]
           initWithDelegate:nil
        personalDataManager:personal_data_manager_.get()
            autofillProfile:profile_.get()
          isMigrationPrompt:NO];
    CreateController();
    CheckController();

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  LegacyInfobarEditAddressProfileTableViewController*
  CreateLegacyInfobarEditAddressProfileTableViewController() {
    LegacyInfobarEditAddressProfileTableViewController* viewController =
        [[LegacyInfobarEditAddressProfileTableViewController alloc]
            initWithModalDelegate:delegate_modal_mock_];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:viewController
                settingsView:NO];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    autofill_profile_edit_mediator_.consumer =
        autofill_profile_edit_table_view_controller_;
    return viewController;
  }

  void CreateAccountProfile() {
    [autofill_profile_edit_table_view_controller_ setAccountProfile:YES];

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  void TestModelRowsAndButtons(TableViewModel* model,
                               NSString* expectedFooterText,
                               NSString* expectedButtonText) {
    NSString* countryCode = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::FieldType::ADDRESS_HOME_COUNTRY));

    std::vector<std::pair<autofill::FieldType, std::u16string>> expected_values;

    for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
      const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];

      if (!FieldIsUsedInAddress(field.autofillType, countryCode)) {
        continue;
      }

      expected_values.push_back(
          {field.autofillType,
           profile_->GetInfo(field.autofillType,
                             GetApplicationContext()->GetApplicationLocale())});
    }

    EXPECT_EQ(1, [model numberOfSections]);
    EXPECT_EQ(expected_values.size() + 2,
              (size_t)[model numberOfItemsInSection:0]);

    for (size_t row = 0; row < expected_values.size(); row++) {
      if (expected_values[row].first == autofill::ADDRESS_HOME_COUNTRY) {
        TableViewMultiDetailTextItem* countryCell =
            static_cast<TableViewMultiDetailTextItem*>(
                GetTableViewItem(0, row));
        EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[row].second),
                    countryCell.trailingDetailText);
        continue;
      }
      TableViewTextEditItem* cell =
          static_cast<TableViewTextEditItem*>(GetTableViewItem(0, row));
      EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[row].second),
                  cell.textFieldValue);
    }

    TableViewTextItem* footerCell = static_cast<TableViewTextItem*>(
        GetTableViewItem(0, expected_values.size()));
    EXPECT_NSEQ(footerCell.text, expectedFooterText);

    TableViewTextButtonItem* buttonCell = static_cast<TableViewTextButtonItem*>(
        GetTableViewItem(0, expected_values.size() + 1));
    EXPECT_NSEQ(buttonCell.buttonText, expectedButtonText);
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return CreateLegacyInfobarEditAddressProfileTableViewController();
  }

  AutofillProfileEditTableViewController*
      autofill_profile_edit_table_view_controller_;
  AutofillProfileEditMediator* autofill_profile_edit_mediator_;
  std::unique_ptr<autofill::AutofillProfile> profile_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  id delegate_modal_mock_;
};

// Tests the edit view initialisation for the save prompt of an account profile.
TEST_F(LegacyInfobarEditAddressProfileTableViewControllerTest,
       TestEditForAccountProfile) {
  CreateAccountProfile();

  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, kTestSyncingEmail);
  TestModelRowsAndButtons(
      [controller() tableViewModel], expected_footer_text,
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

class LegacyInfobarEditAddressProfileTableViewControllerMigrationPromptTest
    : public LegacyInfobarEditAddressProfileTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    LegacyInfobarEditAddressProfileTableViewController* viewController =
        CreateLegacyInfobarEditAddressProfileTableViewController();
    [viewController setMigrationPrompt:YES];
    return viewController;
  }
};

// Tests the edit view initialisation for the migration prompt to account.
TEST_F(LegacyInfobarEditAddressProfileTableViewControllerMigrationPromptTest,
       TestMigrationPrompt) {
  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, kTestSyncingEmail);
  TestModelRowsAndButtons(
      [controller() tableViewModel], expected_footer_text,
      l10n_util::GetNSString(
          IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL));
}

}  // namespace
