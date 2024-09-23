// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const char16_t kTestSyncingEmail[] = u"test@email.com";

class AutofillSettingsProfileEditTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
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

  LegacyChromeTableViewController* InstantiateController() override {
    AutofillSettingsProfileEditTableViewController* viewController =
        [[AutofillSettingsProfileEditTableViewController alloc]
                            initWithDelegate:nil
            shouldShowMigrateToAccountButton:NO
                                   userEmail:nil];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:nil
                  controller:viewController
                settingsView:YES];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    autofill_profile_edit_mediator_.consumer =
        autofill_profile_edit_table_view_controller_;
    return viewController;
  }

  AutofillProfileEditTableViewController*
      autofill_profile_edit_table_view_controller_;
  AutofillProfileEditMediator* autofill_profile_edit_mediator_;
  std::unique_ptr<autofill::AutofillProfile> profile_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  id delegate_mock_;
};

// Default test case of no addresses or credit cards.
TEST_F(AutofillSettingsProfileEditTableViewControllerTest, TestInitialization) {
  TableViewModel* model = [controller() tableViewModel];

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(10, [model numberOfItemsInSection:0]);
}

// TODO(crbug.com/40233297): Merge into main test fixture.
class AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled
    : public AutofillSettingsProfileEditTableViewControllerTest {
 protected:
  AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled() {}

  LegacyChromeTableViewController* InstantiateController() override {
    AutofillSettingsProfileEditTableViewController* viewController =
        [[AutofillSettingsProfileEditTableViewController alloc]
                            initWithDelegate:nil
            shouldShowMigrateToAccountButton:NO
                                   userEmail:base::SysUTF16ToNSString(
                                                 kTestSyncingEmail)];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:viewController
                settingsView:YES];
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

  // Tests the data in the address section.
  void TestViewData() {
    TableViewModel* model = [controller() tableViewModel];

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

    EXPECT_EQ(expected_values.size(), (size_t)[model numberOfItemsInSection:0]);
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
  }
};

// Adding an account address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestAccountProfileView) {
  CreateAccountProfile();
  EXPECT_EQ(2, [[controller() tableViewModel] numberOfSections]);
  TestViewData();
}

// Adding an address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestProfileView) {
  EXPECT_EQ(1, [[controller() tableViewModel] numberOfSections]);
  TestViewData();
}

// Tests the footer text of the view controller for the address profiles with
// source kAccount.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestFooterTextWithEmail) {
  CreateAccountProfile();
  TableViewModel* model = [controller() tableViewModel];

  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT, kTestSyncingEmail);
  TableViewLinkHeaderFooterItem* footer = [model footerForSectionIndex:1];
  EXPECT_NSEQ(expected_footer_text, footer.text);
}

class AutofillSettingsProfileEditTableViewControllerWithMigrationButtonTest
    : public AutofillSettingsProfileEditTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    AutofillSettingsProfileEditTableViewController* viewController =
        [[AutofillSettingsProfileEditTableViewController alloc]
                            initWithDelegate:nil
            shouldShowMigrateToAccountButton:YES
                                   userEmail:base::SysUTF16ToNSString(
                                                 kTestSyncingEmail)];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:viewController
                settingsView:YES];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    autofill_profile_edit_mediator_.consumer =
        autofill_profile_edit_table_view_controller_;
    return viewController;
  }
};

// Tests the number of sections and the number of items in the sections.
TEST_F(AutofillSettingsProfileEditTableViewControllerWithMigrationButtonTest,
       TestElementsInView) {
  TableViewModel* model = [controller() tableViewModel];
  int rowCnt = 12;

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(rowCnt, [model numberOfItemsInSection:0]);
  NSString* migrateButtonDescription = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_BUTTON_DESCRIPTION,
      kTestSyncingEmail);
  TableViewItem* descriptionItem = GetTableViewItem(0, rowCnt - 2);
  EXPECT_NSEQ(
      static_cast<SettingsImageDetailTextItem*>(descriptionItem).detailText,
      migrateButtonDescription);
  EXPECT_NSEQ(
      static_cast<TableViewTextItem*>(GetTableViewItem(0, rowCnt - 1)).text,
      l10n_util::GetNSString(
          IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_BUTTON_TITLE));
}

}  // namespace
