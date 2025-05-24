// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/autofill_edit_profile_button_footer_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/autofill_profile_edit_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
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
                             AutofillSaveProfilePromptMode::kMigrateProfile)
           addManualAddress:NO];
    autofill_profile_edit_table_view_controller_ =
        [[AutofillProfileEditTableViewController alloc]
            initWithDelegate:autofill_profile_edit_mediator_
                   userEmail:base::SysUTF16ToNSString(kTestSyncingEmail)
                  controller:controller()
                settingsView:GetParam().is_settings
            addManualAddress:NO];
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
    street_address_ = base::SysUTF16ToNSString(
        profile_->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS));
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
    if (section == 1 && index == 4) {
      return base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(
                 GetTableViewItem(section, index))
          .trailingDetailText;
    }

    return base::apple::ObjCCastStrict<AutofillProfileEditItem>(
               GetTableViewItem(section, index))
        .textFieldValue;
  }

  NSAttributedString* GetErrorFooterString(int num_of_errors,
                                           bool show_update_string) {
    NSString* error = l10n_util::GetPluralNSStringF(
        IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
        num_of_errors);

    NSString* finalErrorString = [NSString
        stringWithFormat:
            @"%@\n%@", error,
            l10n_util::GetNSStringF(
                show_update_string
                    ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
                    : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
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
  NSString* street_address_;
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
  int numOfSections;
  if (test_case.is_settings) {
    numOfSections = test_case.account_profile ? 4 : 3;
  } else {
    numOfSections = test_case.account_profile ||
                            test_case.prompt_mode ==
                                AutofillSaveProfilePromptMode::kMigrateProfile
                        ? 5
                        : 4;
  }

  EXPECT_EQ(NumberOfSections(), numOfSections);
  EXPECT_EQ(NumberOfItemsInSection(0), 2);
  EXPECT_EQ(NumberOfItemsInSection(1), 5);
  EXPECT_EQ(NumberOfItemsInSection(2), 2);

  EXPECT_NSEQ(GetFieldValue(0, 0), full_name_);
  EXPECT_NSEQ(GetFieldValue(0, 1), company_name_);
  EXPECT_NSEQ(GetFieldValue(1, 0), street_address_);
  EXPECT_NSEQ(GetFieldValue(1, 1), city_);
  EXPECT_NSEQ(GetFieldValue(1, 2), state_);
  EXPECT_NSEQ(GetFieldValue(1, 3), zip_);
  EXPECT_NSEQ(GetFieldValue(1, 4), country_);
  EXPECT_NSEQ(GetFieldValue(2, 0), phone_home_whole_number_);
  EXPECT_NSEQ(GetFieldValue(2, 1), email_);

  if (test_case.is_settings) {
    if (test_case.account_profile) {
      EXPECT_EQ(NumberOfItemsInSection(3), 0);
      NSString* expected_footer_text = l10n_util::GetNSStringF(
          IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
          kTestSyncingEmail);
      CheckSectionFooter(expected_footer_text, 3);
    }
  } else {
    if (test_case.account_profile ||
        test_case.prompt_mode ==
            AutofillSaveProfilePromptMode::kMigrateProfile) {
      int expected_text_id =
          (test_case.prompt_mode ==
                   AutofillSaveProfilePromptMode::kUpdateProfile
               ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
               : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER);
      // Check footer text in the save prompt.
      CheckSectionFooter(
          l10n_util::GetNSStringF(expected_text_id, kTestSyncingEmail),
          numOfSections - 2);
      EXPECT_EQ(NumberOfItemsInSection(4), 0);
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

    AutofillEditProfileButtonFooterItem* footer =
        static_cast<AutofillEditProfileButtonFooterItem*>([[controller()
            tableViewModel] footerForSectionIndex:numOfSections - 1]);

    // Check button in the save prompt.
    EXPECT_NSEQ(footer.buttonText, l10n_util::GetNSString(button_id));
    EXPECT_EQ(NumberOfItemsInSection(3), 0);
  }
}

// Test the contents of the view when the value requirements fail.
TEST_P(AutofillProfileEditTableViewControllerTest, TestRequirements) {
  TableViewTextEditItem* city_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(1, 1));
  // Remove the city field value.
  city_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:city_item];

  auto test_case = GetParam();
  BOOL show_update_string =
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kUpdateProfile ||
      test_case.is_settings;
  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    int sectionIndex = test_case.is_settings ? (NumberOfSections() - 1)
                                             : (NumberOfSections() - 2);
    TableViewAttributedStringHeaderFooterItem* footer_item =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:sectionIndex]);
    EXPECT_NSEQ(GetErrorFooterString(1, show_update_string),
                footer_item.attributedString);
  }

  if (!test_case.is_settings) {
    AutofillEditProfileButtonFooterItem* button_item =
        static_cast<AutofillEditProfileButtonFooterItem*>([[controller()
            tableViewModel] footerForSectionIndex:NumberOfSections() - 1]);
    EXPECT_EQ(button_item.enabled,
              !test_case.account_profile &&
                  test_case.prompt_mode !=
                      AutofillSaveProfilePromptMode::kMigrateProfile);
  }

  TableViewTextEditItem* zip_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(1, 3));
  // Remove the zip field value.
  zip_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:zip_item];

  // Check that the error message has been updated.
  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    int sectionIndex = test_case.is_settings ? (NumberOfSections() - 1)
                                             : (NumberOfSections() - 2);
    TableViewAttributedStringHeaderFooterItem* footer_item =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:sectionIndex]);
    EXPECT_NSEQ(GetErrorFooterString(2, show_update_string),
                footer_item.attributedString);
  }
}

// Tests the items in the view when the country value changes.
TEST_P(AutofillProfileEditTableViewControllerTest,
       TestItemsOnCountrySelection) {
  auto test_case = GetParam();
  TableViewTextEditItem* city_item =
      static_cast<TableViewTextEditItem*>(GetTableViewItem(1, 1));
  // Remove the city field value.
  city_item.textFieldValue = @"";
  [autofill_profile_edit_table_view_controller_
      tableViewItemDidChange:city_item];

  BOOL show_update_string =
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kUpdateProfile ||
      test_case.is_settings;
  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    int sectionIndex = test_case.is_settings ? (NumberOfSections() - 1)
                                             : (NumberOfSections() - 2);
    TableViewAttributedStringHeaderFooterItem* footer_item =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:sectionIndex]);
    EXPECT_NSEQ(GetErrorFooterString(1, show_update_string),
                footer_item.attributedString);
  }

  [autofill_profile_edit_table_view_controller_
      tableViewItemDidEndEditing:city_item];

  CountryItem* countryItem =
      [[CountryItem alloc] initWithType:kItemTypeEnumZero];
  countryItem.countryCode = @"DE";
  countryItem.text = @"Germany";

  [autofill_profile_edit_mediator_ didSelectCountry:countryItem];

  int numOfSections;
  if (test_case.is_settings) {
    numOfSections = test_case.account_profile ? 4 : 3;
  } else {
    numOfSections = test_case.account_profile ||
                            test_case.prompt_mode ==
                                AutofillSaveProfilePromptMode::kMigrateProfile
                        ? 5
                        : 4;
  }

  EXPECT_EQ(NumberOfSections(), numOfSections);
  EXPECT_EQ(NumberOfItemsInSection(0), 2);
  EXPECT_EQ(NumberOfItemsInSection(1), 5);
  EXPECT_EQ(NumberOfItemsInSection(2), 2);

  if (test_case.account_profile ||
      test_case.prompt_mode == AutofillSaveProfilePromptMode::kMigrateProfile) {
    int sectionIndex = test_case.is_settings ? (NumberOfSections() - 1)
                                             : (NumberOfSections() - 2);
    TableViewAttributedStringHeaderFooterItem* footer_item =
        static_cast<TableViewAttributedStringHeaderFooterItem*>(
            [[controller() tableViewModel] footerForSectionIndex:sectionIndex]);
    EXPECT_NSEQ(GetErrorFooterString(1, show_update_string),
                footer_item.attributedString);
  }
}
