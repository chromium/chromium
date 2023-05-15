// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import <memory>
#import "base/feature_list.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char16_t kTestSyncingEmail[] = u"test@email.com";

class AutofillSettingsProfileEditTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    delegate_mock_ = OCMProtocolMock(
        @protocol(AutofillProfileEditTableViewControllerDelegate));
    CreateController();
    CheckController();
    CreateProfileData();

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  ChromeTableViewController* InstantiateController() override {
    AutofillSettingsProfileEditTableViewController* viewController =
        [[AutofillSettingsProfileEditTableViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:delegate_mock_
                   userEmail:nil
                  controller:viewController
                settingsView:YES];
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

  AutofillProfileEditTableViewController*
      autofill_profile_edit_table_view_controller_;
  id delegate_mock_;
};

// Default test case of no addresses or credit cards.
TEST_F(AutofillSettingsProfileEditTableViewControllerTest, TestInitialization) {
  TableViewModel* model = [controller() tableViewModel];
  int rowCnt =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHonorificPrefixes)
          ? 11
          : 10;

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(rowCnt, [model numberOfItemsInSection:0]);
}

// Adding a single address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTest, TestOneProfile) {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAccountProfilesUnionView)) {
    // The test is incompatible with the feature as the country is a selection
    // field.
    // TODO(crbug.com/1423319): Cleanup
    return;
  }

  TableViewModel* model = [controller() tableViewModel];

  autofill::AutofillProfile profile = autofill::test::GetFullProfile2();
  std::vector<std::u16string> expected_values;

  for (size_t i = 0; i < std::size(kProfileFieldsToDisplay); ++i) {
    const AutofillProfileFieldDisplayInfo& field = kProfileFieldsToDisplay[i];
    if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
      continue;
    }

    expected_values.push_back(profile.GetRawInfo(field.autofillType));
  }

  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(expected_values.size(), (size_t)[model numberOfItemsInSection:0]);

  for (size_t row = 0; row < expected_values.size(); row++) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(0, row));
    EXPECT_NSEQ(base::SysUTF16ToNSString(expected_values[row]),
                cell.textFieldValue);
  }
}

class AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled
    : public AutofillSettingsProfileEditTableViewControllerTest {
 protected:
  AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillAccountProfilesUnionView);
  }

  ChromeTableViewController* InstantiateController() override {
    AutofillSettingsProfileEditTableViewController* viewController =
        [[AutofillSettingsProfileEditTableViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:delegate_mock_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:viewController
                settingsView:YES];
    viewController.handler = autofill_profile_edit_table_view_controller_;
    return viewController;
  }

  void CreateAccountProfile() {
    [autofill_profile_edit_table_view_controller_ setAccountProfile:YES];

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  // Tests the data in the address section.
  void TestViewData(int number_of_sections) {
    TableViewModel* model = [controller() tableViewModel];

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

    EXPECT_EQ(number_of_sections, [model numberOfSections]);
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Adding an account address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestAccountProfileView) {
  CreateAccountProfile();
  TestViewData(2);
}

// Adding an address results in an address section.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestProfileView) {
  TestViewData(1);
}

// Tests the footer text of the view controller for the address profiles with
// source kAccount when
//`autofill::features::kAutofillAccountProfilesUnionView` / is enabled.
TEST_F(AutofillSettingsProfileEditTableViewControllerTestWithUnionViewEnabled,
       TestFooterTextWithEmail) {
  CreateAccountProfile();
  TableViewModel* model = [controller() tableViewModel];

  NSString* expected_footer_text = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT, kTestSyncingEmail);
  TableViewLinkHeaderFooterItem* footer = [model footerForSectionIndex:1];
  EXPECT_NSEQ(expected_footer_text, footer.text);
}

}  // namespace
