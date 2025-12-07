// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Test fixture for testing InfobarSaveAddressProfileViewController class.
class InfobarSaveAddressProfileViewControllerTest : public PlatformTest {
 protected:
  InfobarSaveAddressProfileViewControllerTest()
      : modal_delegate_(OCMProtocolMock(
            @protocol(InfobarSaveAddressProfileModalDelegate))) {}

  InfobarSaveAddressProfileViewController* controller() {
    if (!controller_) {
      controller_ = [[InfobarSaveAddressProfileViewController alloc]
          initWithModalDelegate:modal_delegate_];
    }
    return controller_;
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
        [NSNumber numberWithInt:base::to_underlying(autofill::NAME_FULL)] :
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
        [NSNumber numberWithInt:base::to_underlying(autofill::NAME_FULL)] :
            @[ @"John Doe", @"John H. Doe" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street",
      kUserEmailKey : @"test@gmail.com",
      kIsProfileAnAccountProfileKey : @(true)
    };
    return prefs;
  }

  NSDictionary* GetDataForAddInAccountModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(true),
      kProfileDataDiffKey : @{
        [NSNumber numberWithInt:base::to_underlying(autofill::NAME_FULL)] :
            @[ @"John H. Doe", @"" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street",
      kUserEmailKey : @"test@gmail.com",
      kIsProfileAnAccountProfileKey : @(true)
    };
    return prefs;
  }

  NSDictionary* GetDataForHomeUpdateInAccountModal() {
    NSDictionary* prefs = @{
      kAddressPrefKey : @"",
      kPhonePrefKey : @"",
      kEmailPrefKey : @"",
      kCurrentAddressProfileSavedPrefKey : @(false),
      kIsUpdateModalPrefKey : @(true),
      kProfileDataDiffKey : @{
        [NSNumber numberWithInt:base::to_underlying(autofill::NAME_FULL)] :
            @[ @"John Doe", @"John H. Doe" ]
      },
      kUpdateModalDescriptionKey : @"For John Doe, 345 Spear Street",
      kUserEmailKey : @"test@gmail.com",
      kIsProfileAnAccountProfileKey : @(true),
      kIsProfileAnAccountHomeKey : @(true)
    };
    return prefs;
  }

  // Checks that the button at `button_index` in `stack` has a title displaying
  // `message_id`.
  void CheckButton(int message_id, UIStackView* stack, int button_index) {
    UIView* buttonContainer = stack.arrangedSubviews[button_index];
    ChromeButton* button =
        base::apple::ObjCCastStrict<ChromeButton>(buttonContainer.subviews[0]);
    EXPECT_NSEQ(l10n_util::GetNSString(message_id), button.title);
  }

  id modal_delegate_;
  InfobarSaveAddressProfileViewController* controller_ = nil;
};

// Tests that the save address profile modal has been initialized.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestSaveModalInitialization) {
  InfobarSaveAddressProfileViewController* save_view_controller = controller();
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForSaveModal()];
  [save_view_controller viewDidLoad];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
      save_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(save_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(7u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"Test Envelope Address",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(@"Test Email Address",
              stack.arrangedSubviews[2].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL, stack, 6);
}

// Tests that the update modal has been initialized.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestUpdateModalInitialization) {
  InfobarSaveAddressProfileViewController* update_view_controller =
      controller();
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForUpdateModal()];
  [update_view_controller viewDidLoad];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
      update_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(update_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(8u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"For John Doe, 345 Spear Street",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[1].accessibilityLabel);
  EXPECT_NSEQ(@"John Doe", stack.arrangedSubviews[2].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[4].accessibilityLabel);
  EXPECT_NSEQ(@"John H. Doe", stack.arrangedSubviews[5].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL, stack, 7);
}

// Tests that the save address profile modal has been initialized for saving the
// profile to Google Account.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestSaveInAccountModalInitialization) {
  InfobarSaveAddressProfileViewController* save_view_controller = controller();
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForSaveInAccountModal()];
  [save_view_controller viewDidLoad];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
      save_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(save_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(8u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"Test Envelope Address",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(@"Test Email Address",
              stack.arrangedSubviews[2].accessibilityLabel);
  EXPECT_NSEQ(@"Test Phone Number",
              stack.arrangedSubviews[4].accessibilityLabel);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              u"test@gmail.com"),
      stack.arrangedSubviews[6].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL, stack, 7);
}

// Tests that the save address profile modal has been initialized for migrating
// the profile to Google Account.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestMigrationModalInitialization) {
  InfobarSaveAddressProfileViewController* save_view_controller = controller();
  [save_view_controller
      setupModalViewControllerWithPrefs:GetDataForMigrationModal()];
  [save_view_controller viewDidLoad];

  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE),
              save_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(save_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(5u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(l10n_util::GetNSStringF(
                  IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER,
                  u"test@gmail.com"),
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(@"Test", stack.arrangedSubviews[1].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL,
              stack, 3);
  CheckButton(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_CANCEL_BUTTON_LABEL,
      stack, 4);
}

// Tests that the modal has been initialized  for updating the data of a Google
// Account profile.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestUpdateInAccountModalInitialization) {
  InfobarSaveAddressProfileViewController* update_view_controller =
      controller();
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForUpdateInAccountModal()];
  [update_view_controller viewDidLoad];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
      update_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(update_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(9u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"For John Doe, 345 Spear Street",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[1].accessibilityLabel);
  EXPECT_NSEQ(@"John Doe", stack.arrangedSubviews[2].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[4].accessibilityLabel);
  EXPECT_NSEQ(@"John H. Doe", stack.arrangedSubviews[5].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSStringF(
                  IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                  u"test@gmail.com"),
              stack.arrangedSubviews[7].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL, stack, 8);
}

// Tests that the modal has been initialized for updating the Google account
// profile with new info.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestAddInExistingAccountProfileModalInitialization) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableSupportForHomeAndWork);
  InfobarSaveAddressProfileViewController* update_view_controller =
      controller();
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForAddInAccountModal()];
  [update_view_controller viewDidLoad];

  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_ADD_NEW_INFO_ADDRESS_PROMPT_TITLE),
              update_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(update_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(5u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"For John Doe, 345 Spear Street",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(@"John H. Doe", stack.arrangedSubviews[1].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSStringF(
                  IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
                  u"test@gmail.com"),
              stack.arrangedSubviews[3].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_OK_BUTTON_LABEL,
              stack, 4);
}

// Tests that the modal has been initialized for adding new profile by adding
// new data to an existing home profile.
TEST_F(InfobarSaveAddressProfileViewControllerTest,
       TestUpdateInAccountHomeModalInitialization) {
  base::test::ScopedFeatureList scoped_feature_list(
      autofill::features::kAutofillEnableSupportForHomeAndWork);
  InfobarSaveAddressProfileViewController* update_view_controller =
      controller();
  [update_view_controller
      setupModalViewControllerWithPrefs:GetDataForHomeUpdateInAccountModal()];
  [update_view_controller viewDidLoad];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
      update_view_controller.title);
  UIScrollView* scrollView =
      base::apple::ObjCCastStrict<UIScrollView>(update_view_controller.view);
  UIStackView* stack =
      base::apple::ObjCCastStrict<UIStackView>(scrollView.subviews[0]);
  ASSERT_EQ(9u, stack.arrangedSubviews.count);
  EXPECT_NSEQ(@"For John Doe, 345 Spear Street",
              stack.arrangedSubviews[0].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[1].accessibilityLabel);
  EXPECT_NSEQ(@"John Doe", stack.arrangedSubviews[2].accessibilityLabel);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL),
              stack.arrangedSubviews[4].accessibilityLabel);
  EXPECT_NSEQ(@"John H. Doe", stack.arrangedSubviews[5].accessibilityLabel);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_AUTOFILL_ADDRESS_HOME_RECORD_TYPE_NOTICE,
                              u"test@gmail.com"),
      stack.arrangedSubviews[7].accessibilityLabel);
  CheckButton(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL, stack, 8);
}
