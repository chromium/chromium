// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/autofill_edit_profile_bottom_sheet_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class AutofillEditProfileBottomSheetTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    delegate_mock_ = OCMProtocolMock(
        @protocol(AutofillProfileEditTableViewControllerDelegate));
    CreateController();
    CheckController();
    CreateProfileData();

    // Reload the model so that the changes are propogated.
    [controller() loadModel];
  }

  LegacyChromeTableViewController* InstantiateController() override {
    AutofillEditProfileBottomSheetTableViewController* viewController =
        [[AutofillEditProfileBottomSheetTableViewController alloc]
            initWithStyle:UITableViewStylePlain];
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
        setHomeAddressDependentLocality:
            base::SysUTF16ToNSString(
                profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressCity:base::SysUTF16ToNSString(profile.GetRawInfo(
                               autofill::ADDRESS_HOME_CITY))];
    [autofill_profile_edit_table_view_controller_
        setHomeAddressAdminLevel2:base::SysUTF16ToNSString(profile.GetRawInfo(
                                      autofill::ADDRESS_HOME_ADMIN_LEVEL2))];
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

// Tests that there are no requirement checks for the profiles saved to sync.
TEST_F(AutofillEditProfileBottomSheetTableViewControllerTest,
       TestNoRequirements) {
  [autofill_profile_edit_table_view_controller_ setLine1Required:YES];
  [autofill_profile_edit_table_view_controller_ setCityRequired:YES];
  [autofill_profile_edit_table_view_controller_ setStateRequired:YES];
  [autofill_profile_edit_table_view_controller_ setZipRequired:NO];

  TableViewTextButtonItem* button_item = static_cast<TableViewTextButtonItem*>(
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
  EXPECT_EQ(button_item.enabled, YES);
}

}  // namespace
