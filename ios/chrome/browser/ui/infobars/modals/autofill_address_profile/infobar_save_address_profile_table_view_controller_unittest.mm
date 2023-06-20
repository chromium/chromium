// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing InfobarSaveAddressProfileTableViewController class.
class InfobarSaveAddressProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  InfobarSaveAddressProfileTableViewControllerTest()
      : modal_delegate_(OCMProtocolMock(
            @protocol(InfobarSaveAddressProfileModalDelegate))) {}

  ChromeTableViewController* InstantiateController() override {
    return [[InfobarSaveAddressProfileTableViewController alloc]
        initWithModalDelegate:modal_delegate_];
  }

  NSDictionary* GetDataForSaveModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"Test Envelope Address",
      kPhonePrefKey : @"Test Phone Number",
      kEmailPrefKey : @"Test Email Address",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(false),
      kProfileDataDiffKey : @{},
      kUpdateModalDescriptionKey : @"",
    };
    return prefs;
  }

  NSDictionary* GetDataForSaveInAccountModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"Test Envelope Address",
      kPhonePrefKey : @"Test Phone Number",
      kEmailPrefKey : @"Test Email Address",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(false),
      kProfileDataDiffKey : @{},
      kUpdateModalDescriptionKey : @"",
      kUserEmailKey : @"test@gmail.com",
      kIsProfileAnAccountProfileKey : @(true)
    };
    return prefs;
  }

  NSDictionary* GetDataForMigrationModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(false),
      kProfileDataDiffKey : @{},
      kUpdateModalDescriptionKey : @"",
      kUserEmailKey : @"test@gmail.com",
      kIsMigrationToAccountKey : @(true),
      kProfileDescriptionForMigrationPromptKey : @"Test"
    };
    return prefs;
  }

  NSDictionary* GetDataForUpdateModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(true),
      kProfileDataDiffKey : @{
        [NSNumber numberWithInt:AutofillUITypeNameFullWithHonorificPrefix] :
            @[ @"John Doe", @"John H. Doe" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street"
    };
    return prefs;
  }

  NSDictionary* GetDataForUpdateInAccountModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(true),
      kProfileDataDiffKey : @{
        [NSNumber numberWithInt:AutofillUITypeNameFullWithHonorificPrefix] :
            @[ @"John Doe", @"John H. Doe" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street",
      kUserEmailKey : @"test@gmail.com",
      kIsProfileAnAccountProfileKey : @(true)
    };
    return prefs;
  }

  id modal_delegate_;
};

// Tests that the save address profile modal has been initialized.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestSaveModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* save_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForSaveModal()];
  [save_view_controller viewDidLoad];

  CheckTitleWithId(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));
  CheckTextCellText(@"Test Envelope Address", 0, 0);
  CheckTextCellText(@"Test Email Address", 0, 1);
  CheckTextCellText(@"Test Phone Number", 0, 2);
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL, 0, 3);
}

// Tests that the update modal has been initialized.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestUpdateModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* update_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForUpdateModal()];
  [update_view_controller viewDidLoad];

  CheckTitleWithId(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(6, NumberOfItemsInSection(0));
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL, 0, 5);
}

// Tests that the save address profile modal has been initialized for saving the
// profile to Google Account.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestSaveInAccountModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* save_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForSaveInAccountModal()];
  [save_view_controller viewDidLoad];

  CheckTitleWithId(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(5, NumberOfItemsInSection(0));
  CheckTextCellText(@"Test Envelope Address", 0, 0);
  CheckTextCellText(@"Test Email Address", 0, 1);
  CheckTextCellText(@"Test Phone Number", 0, 2);
  CheckTextCellText(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              u"test@gmail.com"),
      0, 3);
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL, 0, 4);
}

// Tests that the save address profile modal has been initialized for migrating
// the profile to Google Account.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestMigrationModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* save_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForMigrationModal()];
  [save_view_controller viewDidLoad];

  CheckTitleWithId(IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));
  CheckTextCellText(
      @"This address is currently saved to Chrome. To use it across Google "
      @"products, save it in your Google Account, test@gmail.com.",
      0, 0);
  CheckTextCellText(@"Test", 0, 1);
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL, 0, 2);
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_CANCEL_BUTTON_LABEL, 0,
      3);
}

// Tests that the modal has been initialized  for updating the data of a Google
// Account profile.
TEST_F(InfobarSaveAddressProfileTableViewControllerTest,
       TestUpdateInAccountModalInitialization) {
  CreateController();
  CheckController();
  InfobarSaveAddressProfileTableViewController* update_view_controller =
      base::mac::ObjCCastStrict<InfobarSaveAddressProfileTableViewController>(
          controller());
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForUpdateInAccountModal()];
  [update_view_controller viewDidLoad];

  CheckTitleWithId(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(7, NumberOfItemsInSection(0));
  CheckTextCellText(l10n_util::GetNSStringF(
                        IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                        u"test@gmail.com"),
                    0, 5);
  CheckTextButtonCellButtonTextWithId(
      IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL, 0, 6);
}
