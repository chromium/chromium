// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_table_view_controller.h"

#import <memory>
#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const char16_t kTestSyncingEmail[] = u"test@email.com";

class InfobarEditAddressProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    delegate_mock_ = OCMProtocolMock(
        @protocol(AutofillProfileEditTableViewControllerDelegate));
    delegate_modal_mock_ = OCMProtocolMock(@protocol(InfobarModalDelegate));
    CreateController();
    CheckController();
    CreateProfileData();

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  ChromeTableViewController* InstantiateController() override {
    InfobarEditAddressProfileTableViewController* viewController =
        [[InfobarEditAddressProfileTableViewController alloc]
            initWithModalDelegate:delegate_modal_mock_];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:delegate_mock_
                   userEmail:nil
                  controller:viewController
                settingsView:NO];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    return viewController;
  }

  void CreateProfileData() {
    autofill::AutofillProfile profile = autofill::test::GetFullProfile2();
    [autofill_profile_edit_table_view_controller_
        setHonorificPrefix:base::SysUTF16ToNSString(profile.GetRawInfo(
                               autofill::NAME_HONORIFIC_PREFIX))];
    [autofill_profile_edit_table_view_controller_
        setFullName:base::SysUTF16ToNSString(
                        profile.GetRawInfo(autofill::NAME_FULL))];
    [autofill_profile_edit_table_view_controller_
        setCompanyName:base::SysUTF16ToNSString(
                           profile.GetRawInfo(autofill::COMPANY_NAME))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressLine1:base::SysUTF16ToNSString(profile.GetRawInfo(
                                autofill::ADDRESS_HOME_LINE1))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressLine2:base::SysUTF16ToNSString(profile.GetRawInfo(
                                autofill::ADDRESS_HOME_LINE2))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressCity:base::SysUTF16ToNSString(profile.GetRawInfo(
                               autofill::ADDRESS_HOME_CITY))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressState:base::SysUTF16ToNSString(profile.GetRawInfo(
                                autofill::ADDRESS_HOME_STATE))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressZip:base::SysUTF16ToNSString(
                              profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressCountry:base::SysUTF16ToNSString(profile.GetRawInfo(
                                  autofill::ADDRESS_HOME_COUNTRY))];
    [autofill_profile_edit_table_view_controller_
        setHomePhoneWholeNumber:base::SysUTF16ToNSString(profile.GetRawInfo(
                                    autofill::PHONE_HOME_WHOLE_NUMBER))];
    [autofill_profile_edit_table_view_controller_
        setEmailAddress:base::SysUTF16ToNSString(
                            profile.GetRawInfo(autofill::EMAIL_ADDRESS))];
  }

  // Tests that the save button behaviour changes as the requirements change
  // depending on the view.
  void TestRequirements(bool account_profile_or_migration_prompt) {
    [autofill_profile_edit_table_view_controller_ setLine1Required:YES];
    [autofill_profile_edit_table_view_controller_ setCityRequired:YES];
    [autofill_profile_edit_table_view_controller_ setStateRequired:YES];
    [autofill_profile_edit_table_view_controller_ setZipRequired:NO];

    TableViewTextButtonItem* button_item =
        static_cast<TableViewTextButtonItem*>(
            GetTableViewItem(0, NumberOfItemsInSection(0) - 1));
    EXPECT_EQ(button_item.enabled, YES);

    TableViewTextEditItem* zip_item =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 6));
    zip_item.textFieldValue = @"";
    [autofill_profile_edit_table_view_controller_
        tableViewItemDidChange:zip_item];
    // Since, zip was set as not required, the button should be enabled.
    EXPECT_EQ(button_item.enabled, YES);

    TableViewTextEditItem* name_item =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 0));
    name_item.textFieldValue = @"";
    [autofill_profile_edit_table_view_controller_
        tableViewItemDidChange:name_item];
    // Should be still enabled.
    EXPECT_EQ(button_item.enabled, YES);

    TableViewTextEditItem* city_item =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 4));
    city_item.textFieldValue = @"";
    [autofill_profile_edit_table_view_controller_
        tableViewItemDidChange:city_item];
    // Should not be enabled for account profile or migration prompt.
    EXPECT_EQ(button_item.enabled, !account_profile_or_migration_prompt);
  }

  AutofillProfileEditTableViewController*
      autofill_profile_edit_table_view_controller_;
  id delegate_mock_;
  id delegate_modal_mock_;
};

// Tests that there are no requirement checks for the profiles saved to sync.
TEST_F(InfobarEditAddressProfileTableViewControllerTest, TestNoRequirements) {
  TestRequirements(NO);
}

// TODO(crbug.com/1348294): Merge into main test fixture.
class InfobarEditAddressProfileTableViewControllerTestWithUnionViewEnabled
    : public InfobarEditAddressProfileTableViewControllerTest {
 protected:
  InfobarEditAddressProfileTableViewControllerTestWithUnionViewEnabled() {}

  InfobarEditAddressProfileTableViewController*
  CreateInfobarEditAddressProfileTableViewController() {
    InfobarEditAddressProfileTableViewController* viewController =
        [[InfobarEditAddressProfileTableViewController alloc]
            initWithModalDelegate:delegate_modal_mock_];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:delegate_mock_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:viewController
                settingsView:NO];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    return viewController;
  }

  ChromeTableViewController* InstantiateController() override {
    return CreateInfobarEditAddressProfileTableViewController();
  }

  void CreateAccountProfile() {
    [autofill_profile_edit_table_view_controller_ setAccountProfile:YES];

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  void TestModelRowsAndButtons(TableViewModel* model,
                               NSString* expectedFooterText,
                               NSString* expectedButtonText) {
    autofill::AutofillProfile profile = autofill::test::GetFullProfile2();
    std::vector<std::pair<autofill::ServerFieldType, std::u16string>>
        expected_values;

    for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
      const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];
      if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
          !base::FeatureList::IsEnabled(
              autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
        continue;
      }

      expected_values.push_back(
          {field.autofillType, profile.GetRawInfo(field.autofillType)});
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
};

// Tests the edit view initialisation for the save prompt of an account profile.
TEST_F(InfobarEditAddressProfileTableViewControllerTestWithUnionViewEnabled,
       TestEditForAccountProfile) {
  CreateAccountProfile();

  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, kTestSyncingEmail);
  TestModelRowsAndButtons(
      [controller() tableViewModel], expected_footer_text,
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// Tests that the save in account prompt runs requirement checks.
TEST_F(InfobarEditAddressProfileTableViewControllerTestWithUnionViewEnabled,
       TestRequirements) {
  CreateAccountProfile();
  TestRequirements(YES);
}

class InfobarEditAddressProfileTableViewControllerMigrationPromptTest
    : public InfobarEditAddressProfileTableViewControllerTestWithUnionViewEnabled {
 protected:
  ChromeTableViewController* InstantiateController() override {
    InfobarEditAddressProfileTableViewController* viewController =
        CreateInfobarEditAddressProfileTableViewController();
    [viewController setMigrationPrompt:YES];
    return viewController;
  }
};

// Tests the edit view initialisation for the migration prompt to account.
TEST_F(InfobarEditAddressProfileTableViewControllerMigrationPromptTest,
       TestMigrationPrompt) {
  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER, kTestSyncingEmail);
  TestModelRowsAndButtons(
      [controller() tableViewModel], expected_footer_text,
      l10n_util::GetNSString(
          IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL));
}

// Tests that the migration prompt runs requirement checks.
TEST_F(InfobarEditAddressProfileTableViewControllerMigrationPromptTest,
       TestRequirements) {
  TestRequirements(YES);
}

}  // namespace
