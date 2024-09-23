// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/autofill_profile_edit_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char16_t kTestSyncingEmail[] = u"test@email.com";

struct AutofillProfileEditTableViewControllerTestCase {
  // Determines the objective of the prompt shown.
  AutofillSaveProfilePromptMode prompt_mode;
  // Yes, if the profile is an account profile.
  bool account_profile;
  // Yes, if the caller is from settings.
  bool is_settings;
};

class AutofillProfileEditTableViewControllerTest
    : public LegacyChromeTableViewControllerTest,
      public ::testing::WithParamInterface<
          AutofillProfileEditTableViewControllerTestCase> {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
    [controller() loadModel];
    CheckController();
    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();
    profile_ = std::make_unique<autofill::AutofillProfile>(
        autofill::test::GetFullProfile2());
    autofill_profile_edit_mediator_ = [[AutofillProfileEditMediator alloc]
           initWithDelegate:nil
        personalDataManager:personal_data_manager_.get()
            autofillProfile:profile_.get()
          isMigrationPrompt:(GetParam().prompt_mode ==
                             AutofillSaveProfilePromptMode::kMigrateProfile)];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:controller()
                settingsView:GetParam().is_settings];
    autofill_profile_edit_mediator_.consumer =
        autofill_profile_edit_table_view_controller_;

    [autofill_profile_edit_table_view_controller_
        setAccountProfile:GetParam().account_profile];
    if (GetParam().prompt_mode ==
        AutofillSaveProfilePromptMode::kMigrateProfile) {
      [autofill_profile_edit_table_view_controller_ setMigrationPrompt:YES];
    }

    CacheProfileDataForComparisons();
    [autofill_profile_edit_table_view_controller_ loadModel];
    if (!GetParam().is_settings) {
      [autofill_profile_edit_table_view_controller_
          loadMessageAndButtonForModalIfSaveOrUpdate:
              (GetParam().prompt_mode ==
               AutofillSaveProfilePromptMode::kUpdateProfile)];
    } else {
      [autofill_profile_edit_table_view_controller_ loadFooterForSettings];
    }
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[LegacyChromeTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }

  void CacheProfileDataForComparisons() {
    full_name_ =
        base::SysUTF16ToNSString(profile_->GetRawInfo(autofill::NAME_FULL));
    company_name_ =
        base::SysUTF16ToNSString(profile_->GetRawInfo(autofill::COMPANY_NAME));
    address_home_line_1_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_LINE1));
    address_home_line_2_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_LINE2));
    city_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_CITY));
    state_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_STATE));
    zip_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_ZIP));
    country_ = base::SysUTF16ToNSString(
        profile_->GetInfo(autofill::ADDRESS_HOME_COUNTRY,
                          GetApplicationContext()->GetApplicationLocale()));
    phone_home_whole_number_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER));
    email_ =
        base::SysUTF16ToNSString(profile_->GetRawInfo(autofill::EMAIL_ADDRESS));
  }

  NSString* GetFieldValue(int section, int index) {
    if (index == 7) {
      return base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(
                 GetTableViewItem(section, index))
          .trailingDetailText;
    }

    return base::apple::ObjCCastStrict<AutofillProfileEditItem>(
               GetTableViewItem(section, index))
        .textFieldValue;
  }

  NSString* GetFooterTextForModal() {
    return base::apple::ObjCCastStrict<TableViewTextItem>(
               GetTableViewItem(0, 10))
        .text;
  }

  NSAttributedString* GetErrorFooterString(int num_of_errors) {
    NSString* error = l10n_util::GetPluralNSStringF(
        IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
        num_of_errors);

    NSString* finalErrorString =
        [NSString stringWithFormat:
                      @"%@\n%@", error,
                      l10n_util::GetNSStringF(
                          IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                          kTestSyncingEmail)];

    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.paragraphSpacing = 12.0f;

    NSMutableAttributedString* attributedText =
        [[NSMutableAttributedString alloc]
            initWithString:finalErrorString
                attributes:@{
                  NSFontAttributeName : [UIFont
                      preferredFontForTextStyle:UIFontTextStyleFootnote],
                  NSForegroundColorAttributeName :
                      [UIColor colorNamed:kTextSecondaryColor],
                  NSParagraphStyleAttributeName : paragraphStyle
                }];
    [attributedText addAttribute:NSForegroundColorAttributeName
                           value:[UIColor colorNamed:kRedColor]
                           range:NSMakeRange(0, error.length)];
    return attributedText;
  }

  AutofillProfileEditTableViewController*
      autofill_profile_edit_table_view_controller_;
  AutofillProfileEditMediator* autofill_profile_edit_mediator_;
  NSString* full_name_;
  NSString* company_name_;
  NSString* address_home_line_1_;
  NSString* address_home_line_2_;
  NSString* city_;
  NSString* state_;
  NSString* zip_;
  NSString* country_;
  NSString* phone_home_whole_number_;
  NSString* email_;
  std::unique_ptr<autofill::AutofillProfile> profile_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    AutofillProfileEditTableViewControllerTest,
    testing::Values(
        // Editing an account profile via settings.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/YES,
            /*is_settings=*/YES},

        // Editing a local profile via settings.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/NO,
            /*is_settings=*/YES},

        // Save Flow via Overlay UI: Editing an account profile.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/YES,
            /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/NO,
            /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing an account profile after showing
        // the update prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kUpdateProfile,
            /*account_profile=*/YES, /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile after showing the
        // update prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kUpdateProfile,
            /*account_profile=*/NO, /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile after showing the
        // migration to account prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kMigrateProfile,
            /*account_profile=*/NO, /*is_settings=*/NO}));

}  // namespace

// Tests the items present in the view.
TEST_P(AutofillProfileEditTableViewControllerTest, TestItems) {
  auto test_case = GetParam();
  bool multiple_sections = (test_case.is_settings && test_case.account_profile);
  EXPECT_EQ(NumberOfSections(), multiple_sections ? 2 : 1);
  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    EXPECT_EQ(NumberOfItemsInSection(0), test_case.is_settings ? 10 : 12);
  } else {
    EXPECT_EQ(NumberOfItemsInSection(0), test_case.is_settings ? 10 : 11);
  }

  EXPECT_NSEQ(GetFieldValue(0, 0), full_name_);
  EXPECT_NSEQ(GetFieldValue(0, 1), company_name_);
  EXPECT_NSEQ(GetFieldValue(0, 2), address_home_line_1_);
  EXPECT_NSEQ(GetFieldValue(0, 3), address_home_line_2_);
  EXPECT_NSEQ(GetFieldValue(0, 4), city_);
  EXPECT_NSEQ(GetFieldValue(0, 5), state_);
  EXPECT_NSEQ(GetFieldValue(0, 6), zip_);
  EXPECT_NSEQ(GetFieldValue(0, 7), country_);
  EXPECT_NSEQ(GetFieldValue(0, 8), phone_home_whole_number_);
  EXPECT_NSEQ(GetFieldValue(0, 9), email_);

  if (!test_case.is_settings) {
    int index = 10;
    if (test_case.account_profile ||
        test_case.prompt_mode ==
            AutofillSaveProfilePromptMode::kMigrateProfile) {
      int expected_text_id =
          (test_case.prompt_mode ==
                   AutofillSaveProfilePromptMode::kUpdateProfile
               ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
               : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER);
      // Check footer text in the save prompt.
      EXPECT_NSEQ(GetFooterTextForModal(),
                  l10n_util::GetNSStringF(expected_text_id, kTestSyncingEmail));
      index = 11;
    }

    int button_id = IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
    if (test_case.prompt_mode ==
        AutofillSaveProfilePromptMode::kUpdateProfile) {
      button_id = IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
    } else if (test_case.prompt_mode ==
               AutofillSaveProfilePromptMode::kMigrateProfile) {
      button_id =
          IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL;
    }

    // Check button in the save prompt.
    CheckTextButtonCellButtonTextWithId(button_id, 0, index);
  }

  // Check contents of the other section if present.
  if (multiple_sections) {
    EXPECT_EQ(NumberOfItemsInSection(1), 0);
    NSString* expected_footer_text = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
        kTestSyncingEmail);
    CheckSectionFooter(expected_footer_text, 1);
  }
}

// Test the contents of the view when the value requirements fail.
TEST_P(AutofillProfileEditTableViewControllerTest, TestRequirements) {
  TableViewTextEditItem* city_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 4));
  // Remove the city field value.
  city_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:city_item];

  auto test_case = GetParam();
  if (test_case.is_settings) {
    if (test_case.account_profile) {
      // Check the error message.
      TableViewAttributedStringHeaderFooterItem* footer =
          static_cast<TableViewAttributedStringHeaderFooterItem*>(
              [[controller() tableViewModel] footerForSectionIndex:1]);
      EXPECT_NSEQ(GetErrorFooterString(1), footer.attributedString);
    }
  } else {
    // The button in the save prompt should be disabled.
    TableViewTextButtonItem* button_item =
        static_cast<TableViewTextButtonItem*>(
            GetTableViewItem(0, NumberOfItemsInSection(0) - 1));
    EXPECT_EQ(button_item.enabled,
              !test_case.account_profile &&
                  test_case.prompt_mode !=
                      AutofillSaveProfilePromptMode::kMigrateProfile);

    // Footer text remains unchanged for the save prompts.
    if (test_case.account_profile ||
        test_case.prompt_mode ==
            AutofillSaveProfilePromptMode::kMigrateProfile) {
      int expected_text_id =
          (test_case.prompt_mode ==
                   AutofillSaveProfilePromptMode::kUpdateProfile
               ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
               : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER);
      EXPECT_NSEQ(GetFooterTextForModal(),
                  l10n_util::GetNSStringF(expected_text_id, kTestSyncingEmail));
    }
  }

  TableViewTextEditItem* zip_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 6));
  // Remove the zip field value.
  zip_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:zip_item];

  if (test_case.is_settings && test_case.account_profile) {
    TableViewAttributedStringHeaderFooterItem* footer =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:1]);
    // Check that the error message has been updated.
    EXPECT_NSEQ(GetErrorFooterString(2), footer.attributedString);
  }
}

// Tests the items in the view when the country value changes.
TEST_P(AutofillProfileEditTableViewControllerTest,
       TestItemsOnCountrySelection) {
  auto test_case = GetParam();
  TableViewTextEditItem* city_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(0, 4));
  // Remove the city field value.
  city_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:city_item];

  // Check the error message is shown.
  if (test_case.is_settings && test_case.account_profile) {
    TableViewAttributedStringHeaderFooterItem* footer =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:1]);
    // Check that the error message has been updated.
    EXPECT_NSEQ(GetErrorFooterString(1), footer.attributedString);
  }

  [autofill_profile_edit_table_view_controller_
      tableViewItemDidEndEditing:city_item];

  CountryItem* countryItem =
      [[CountryItem alloc] initWithType:kItemTypeEnumZero];
  countryItem.countryCode = @"DE";
  countryItem.text = @"Germany";

  [autofill_profile_edit_mediator_ didSelectCountry:countryItem];

  bool multiple_sections = (test_case.is_settings && test_case.account_profile);
  EXPECT_EQ(NumberOfSections(), multiple_sections ? 2 : 1);
  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    EXPECT_EQ(NumberOfItemsInSection(0), test_case.is_settings ? 10 : 12);
  } else {
    EXPECT_EQ(NumberOfItemsInSection(0), test_case.is_settings ? 10 : 11);
  }

  // Check the error message persists.
  if (test_case.is_settings && test_case.account_profile) {
    TableViewAttributedStringHeaderFooterItem* footer =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:1]);
    // Check that the error message has been updated.
    EXPECT_NSEQ(GetErrorFooterString(1), footer.attributedString);
  }
}

class AutofillProfileEditTableViewControllerTestWithDynamicFieldsEnabled
    : public AutofillProfileEditTableViewControllerTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillDynamicallyLoadsFieldsForAddressInput);

    AutofillProfileEditTableViewControllerTest::SetUp();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    AutofillProfileEditTableViewControllerTestWithDynamicFieldsEnabled,
    testing::Values(
        // Editing an account profile via settings.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/YES,
            /*is_settings=*/YES},

        // Editing a local profile via settings.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/NO,
            /*is_settings=*/YES},

        // Save Flow via Overlay UI: Editing an account profile.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/YES,
            /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kNewProfile, /*account_profile=*/NO,
            /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing an account profile after showing
        // the update prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kUpdateProfile,
            /*account_profile=*/YES, /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile after showing the
        // update prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kUpdateProfile,
            /*account_profile=*/NO, /*is_settings=*/NO},

        // Save Flow via Overlay UI: Editing a local profile after showing the
        // migration to account prompt.
        AutofillProfileEditTableViewControllerTestCase{
            AutofillSaveProfilePromptMode::kMigrateProfile,
            /*account_profile=*/NO, /*is_settings=*/NO}));

// Test the sections and items in the section on initialisation as well as when
// country value is changed.
TEST_P(AutofillProfileEditTableViewControllerTestWithDynamicFieldsEnabled,
       SectionsAndItems) {
  auto test_case = GetParam();
  if (test_case.is_settings) {
    EXPECT_EQ(NumberOfSections(), test_case.account_profile ? 4 : 3);
  } else {
    EXPECT_EQ(NumberOfSections(),
              (test_case.account_profile ||
               test_case.prompt_mode ==
                   AutofillSaveProfilePromptMode::kMigrateProfile)
                  ? 5
                  : 4);
  }

  EXPECT_EQ(NumberOfItemsInSection(0), 2);
  EXPECT_EQ(NumberOfItemsInSection(1), 5);
  EXPECT_EQ(NumberOfItemsInSection(2), 2);

  if ((test_case.account_profile ||
       test_case.prompt_mode ==
           AutofillSaveProfilePromptMode::kMigrateProfile)) {
    EXPECT_EQ(NumberOfItemsInSection(3), 0);
    NSString* expected_footer_text = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
        kTestSyncingEmail);
    CheckSectionFooter(expected_footer_text, 3);

    if (!test_case.is_settings) {
      EXPECT_EQ(NumberOfItemsInSection(4), 0);
    }
  }

  CountryItem* countryItem =
      [[CountryItem alloc] initWithType:kItemTypeEnumZero];
  countryItem.countryCode = @"DE";
  countryItem.text = @"Germany";

  [autofill_profile_edit_mediator_ didSelectCountry:countryItem];

  EXPECT_EQ(NumberOfItemsInSection(1), 4);
}
