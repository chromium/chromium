// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_ui_type_util.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
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
          isMigrationPrompt:NO
           addManualAddress:NO];
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
                settingsView:YES
            addManualAddress:NO];
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

  EXPECT_EQ(3, [model numberOfSections]);
  EXPECT_EQ(2, [model numberOfItemsInSection:0]);
  EXPECT_EQ(5, [model numberOfItemsInSection:1]);
  EXPECT_EQ(2, [model numberOfItemsInSection:2]);
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
                settingsView:YES
            addManualAddress:NO];
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

    constexpr auto fieldTypes = std::to_array<autofill::FieldType>(
        {autofill::NAME_FULL, autofill::COMPANY_NAME,
         autofill::ADDRESS_HOME_STREET_ADDRESS, autofill::ADDRESS_HOME_CITY,
         autofill::ADDRESS_HOME_STATE, autofill::ADDRESS_HOME_ZIP,
         autofill::ADDRESS_HOME_COUNTRY, autofill::PHONE_HOME_WHOLE_NUMBER,
         autofill::EMAIL_ADDRESS});

    std::vector<std::pair<autofill::FieldType, std::u16string>> expected_values;
    for (const auto& type : fieldTypes) {
      expected_values.push_back(
          {type, profile_->GetInfo(
                     type, GetApplicationContext()->GetApplicationLocale())});
    }

    size_t totalItems = (size_t)[model numberOfItemsInSection:0] +
                        (size_t)[model numberOfItemsInSection:1] +
                        (size_t)[model numberOfItemsInSection:2];

    EXPECT_EQ(expected_values.size(), totalItems);
    size_t section = 0;
    size_t indexOfItemInSection = 0;
    for (size_t i = 0; i < expected_values.size(); i++) {
      if (indexOfItemInSection ==
          (size_t)[model numberOfItemsInSection:section]) {
        section++;
        indexOfItemInSection = 0;
      }

      if (expected_values[i].first == autofill::ADDRESS_HOME_COUNTRY) {
        TableViewMultiDetailTextItem* countryCell =
            static_cast<TableViewMultiDetailTextItem*>(
                GetTableViewItem(section, indexOfItemInSection));
        EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[i].second),
                    countryCell.trailingDetailText);
        indexOfItemInSection++;
        continue;
      }

      TableViewTextEditItem* cell = static_cast<TableViewTextEditItem*>(
          GetTableViewItem(section, indexOfItemInSection));
      EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[i].second),
                  cell.textFieldValue);
      indexOfItemInSection++;
    }
  }
};

// Adding an account address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestAccountProfileView) {
  CreateAccountProfile();
  EXPECT_EQ(4, [[controller() tableViewModel] numberOfSections]);
  TestViewData();
}

// Adding an address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestProfileView) {
  EXPECT_EQ(3, [[controller() tableViewModel] numberOfSections]);
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
  TableViewLinkHeaderFooterItem* footer = [model footerForSectionIndex:3];
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
                settingsView:YES
            addManualAddress:NO];
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

  EXPECT_EQ(4, [model numberOfSections]);
  EXPECT_EQ(2, [model numberOfItemsInSection:0]);
  EXPECT_EQ(5, [model numberOfItemsInSection:1]);
  EXPECT_EQ(2, [model numberOfItemsInSection:2]);
  EXPECT_EQ(2, [model numberOfItemsInSection:3]);
  NSString* migrateButtonDescription = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_BUTTON_DESCRIPTION,
      kTestSyncingEmail);
  TableViewItem* descriptionItem = GetTableViewItem(3, 0);
  EXPECT_NSEQ(
      static_cast<SettingsImageDetailTextItem*>(descriptionItem).detailText,
      migrateButtonDescription);
  EXPECT_NSEQ(static_cast<TableViewTextItem*>(GetTableViewItem(3, 1)).text,
              l10n_util::GetNSString(
                  IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_BUTTON_ITEM));
}

}  // namespace
