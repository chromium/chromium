// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_timeouts.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Name of the histogram that logs the outcome of the prompt that allows the
// user to set the app as a credential provider.
constexpr char kTurnOnCredentialProviderExtensionPromptOutcomeHistogram[] =
    "IOS.CredentialProviderExtension.TurnOnPromptOutcome.PasswordSettings";

// Helper method that returns the expected title for the managed and unmanaged
// "offer to save passwords" table view items.
NSString* GetExpectedSavePasswordsItemTitle() {
  return l10n_util::GetNSString(IDS_IOS_OFFER_TO_SAVE_PASSWORDS_PASSKEYS);
}

// Helper method that returns the expected title for the passwords in other apps
// table view item.
NSString* GetExpectedPasswordsInOtherAppsItemTitle() {
  if (@available(iOS 18.0, *)) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_PASSKEYS_IN_OTHER_APPS_IOS18);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_PASSKEYS_IN_OTHER_APPS);
  }
}

}  // namespace

class PasswordSettingsViewControllerTest : public PlatformTest {
 protected:
  PasswordSettingsViewControllerTest() { CreateController(); }

  TableViewItem* GetTableViewItem(PasswordSettingsSectionIdentifier section,
                                  int item) {
    return [[controller_ tableViewModel]
        itemAtIndexPath:[NSIndexPath
                            indexPathForItem:item
                                   inSection:GetSectionIndex(section)]];
  }

  bool HasTableViewItem(PasswordSettingsSectionIdentifier section, int item) {
    return [[controller_ tableViewModel]
        hasItemAtIndexPath:[NSIndexPath
                               indexPathForItem:item
                                      inSection:GetSectionIndex(section)]];
  }

  int GetSectionIndex(PasswordSettingsSectionIdentifier section) {
    return [[controller_ tableViewModel] sectionForSectionIdentifier:section];
  }

  void CreateController() {
    controller_ = [[PasswordSettingsViewController alloc] init];

    // Accessing this property will force the table view to be built, making
    // sure it is populated when the tests run.
    [controller_ view];
  }

  PasswordSettingsViewController* controller() { return controller_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  PasswordSettingsViewController* controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PasswordSettingsViewControllerTest, OrdersSectionsCorrectly) {
  // Ensure all sections are visible.
  [controller() setCanBulkMove:YES localPasswordsCount:2];
  [controller() setSavingPasswordsEnabled:YES managedByPolicy:NO];
  [controller() setSavingPasskeysEnabled:YES];
  [controller() setCanChangeGPMPin:YES];
  [controller() setOnDeviceEncryptionState:
                    PasswordSettingsOnDeviceEncryptionStateOptedIn];
  [controller() setCanExportPasswords:YES];
  [controller() setCanDeleteAllCredentials:YES];

  // Verify the order.
  EXPECT_EQ(GetSectionIndex(SectionIdentifierSavePasswordsSwitch), 0);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierBulkMovePasswordsToAccount), 1);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierPasswordsInOtherApps), 2);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierAutomaticPasskeyUpgradesSwitch),
            3);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierGooglePasswordManagerPin), 4);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierOnDeviceEncryption), 5);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierExportPasswordsButton), 6);
  EXPECT_EQ(GetSectionIndex(SectionIdentifierDeleteCredentialsButton), 7);
}

TEST_F(PasswordSettingsViewControllerTest, DisplaysOfferToSavePasswords) {
  TableViewSwitchItem* savePasswordsItem = static_cast<TableViewSwitchItem*>(
      GetTableViewItem(SectionIdentifierSavePasswordsSwitch, /*item=*/0));
  EXPECT_NSEQ(savePasswordsItem.text, GetExpectedSavePasswordsItemTitle());
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysOfferToSavePasswordsManagedByPolicy) {
  [controller() setSavingPasswordsEnabled:NO managedByPolicy:YES];
  TableViewInfoButtonItem* managedSavePasswordsItem =
      static_cast<TableViewInfoButtonItem*>(
          GetTableViewItem(SectionIdentifierSavePasswordsSwitch, 0));
  EXPECT_NSEQ(managedSavePasswordsItem.text,
              GetExpectedSavePasswordsItemTitle());
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysMovePasswordsToAccountButtonWithLocalPasswords) {
  [controller() setCanBulkMove:YES localPasswordsCount:2];

  TableViewDetailTextItem* movePasswordsToAccountDescriptionItem =
      static_cast<TableViewDetailTextItem*>(GetTableViewItem(
          SectionIdentifierBulkMovePasswordsToAccount, /*item=*/0));
  EXPECT_NSEQ(
      movePasswordsToAccountDescriptionItem.text,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_TITLE));

  TableViewTextItem* movePasswordsToAccountButtonItem =
      static_cast<TableViewTextItem*>(GetTableViewItem(
          SectionIdentifierBulkMovePasswordsToAccount, /*item=*/1));
  EXPECT_NSEQ(
      movePasswordsToAccountButtonItem.text,
      l10n_util::GetPluralNSStringF(
          IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_BUTTON, 2));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysPasswordInOtherAppsDisabled) {
  [controller() setPasswordsInOtherAppsEnabled:NO];

  TableViewMultiDetailTextItem* passwords_in_other_apps_item =
      static_cast<TableViewMultiDetailTextItem*>(
          GetTableViewItem(SectionIdentifierPasswordsInOtherApps, /*item=*/0));
  EXPECT_NSEQ(passwords_in_other_apps_item.text,
              GetExpectedPasswordsInOtherAppsItemTitle());
  if (@available(iOS 18, *)) {
    EXPECT_NSEQ(
        passwords_in_other_apps_item.leadingDetailText,
        l10n_util::GetNSString(
            IDS_IOS_PASSWORD_SETTINGS_PASSWORDS_IN_OTHER_APPS_DESCRIPTION));
    EXPECT_FALSE(passwords_in_other_apps_item.trailingDetailText);
    EXPECT_EQ(passwords_in_other_apps_item.accessoryType,
              UITableViewCellAccessoryNone);

    // Check that the "Turn on AutoFill…" button is in the table view.
    EXPECT_TRUE(
        HasTableViewItem(SectionIdentifierPasswordsInOtherApps, /*item=*/1));
  } else {
    EXPECT_FALSE(passwords_in_other_apps_item.leadingDetailText);
    EXPECT_NSEQ(passwords_in_other_apps_item.trailingDetailText,
                l10n_util::GetNSString(IDS_IOS_SETTING_OFF));
    EXPECT_EQ(passwords_in_other_apps_item.accessoryType,
              UITableViewCellAccessoryDisclosureIndicator);

    // Check that the "Turn on AutoFill…" button isn't in the table view.
    EXPECT_FALSE(
        HasTableViewItem(SectionIdentifierPasswordsInOtherApps, /*item=*/1));
  }
}

TEST_F(PasswordSettingsViewControllerTest, DisplaysPasswordInOtherAppsEnabled) {
  [controller() setPasswordsInOtherAppsEnabled:YES];

  TableViewMultiDetailTextItem* passwords_in_other_apps_item =
      static_cast<TableViewMultiDetailTextItem*>(
          GetTableViewItem(SectionIdentifierPasswordsInOtherApps, /*item=*/0));
  EXPECT_NSEQ(passwords_in_other_apps_item.text,
              GetExpectedPasswordsInOtherAppsItemTitle());
  EXPECT_NSEQ(passwords_in_other_apps_item.trailingDetailText,
              l10n_util::GetNSString(IDS_IOS_SETTING_ON));
  EXPECT_EQ(passwords_in_other_apps_item.accessoryType,
            UITableViewCellAccessoryDisclosureIndicator);
  if (@available(iOS 18, *)) {
    EXPECT_NSEQ(
        passwords_in_other_apps_item.leadingDetailText,
        l10n_util::GetNSString(
            IDS_IOS_PASSWORD_SETTINGS_PASSWORDS_IN_OTHER_APPS_DESCRIPTION));
  } else {
    EXPECT_FALSE(passwords_in_other_apps_item.leadingDetailText);
  }

  // Check that the "Turn on AutoFill…" button isn't in the table view.
  EXPECT_FALSE(
      HasTableViewItem(SectionIdentifierPasswordsInOtherApps, /*item=*/1));
}

// Tests that the right histogram is logged when tapping the "Turn on AutoFill…"
// button.
TEST_F(PasswordSettingsViewControllerTest, TurnOnAutoFillButtonMetric) {
  // The "Turn on AutoFill…" button is only available on iOS 18+.
  if (@available(iOS 18.0, *)) {
    [controller() setPasswordsInOtherAppsEnabled:NO];

    // Make sure bucket counts are all initially zero.
    histogram_tester().ExpectTotalCount(
        kTurnOnCredentialProviderExtensionPromptOutcomeHistogram, 0);

    // Simulate a tap on the "Turn on AutoFill…" button.
    [controller() tableView:controller().tableView
        didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:1]];

    // Wait for the histogram to be logged.
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), ^bool() {
          return histogram_tester().GetBucketCount(
                     kTurnOnCredentialProviderExtensionPromptOutcomeHistogram,
                     false) == 1;
        }));

    // Verify that only the expected metric was logged.
    histogram_tester().ExpectUniqueSample(
        kTurnOnCredentialProviderExtensionPromptOutcomeHistogram, false, 1);
  }
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysAutomaticPasskeyUpgradesSwitch) {
  [controller() setSavingPasswordsEnabled:YES managedByPolicy:NO];
  [controller() setSavingPasskeysEnabled:YES];

  TableViewSwitchItem* automaticPasskeyUpgradesSwitch =
      static_cast<TableViewSwitchItem*>(GetTableViewItem(
          SectionIdentifierAutomaticPasskeyUpgradesSwitch, /*item=*/0));
  EXPECT_NSEQ(automaticPasskeyUpgradesSwitch.text,
              l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES));
  EXPECT_NSEQ(automaticPasskeyUpgradesSwitch.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES_SUBTITLE));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysChangeGPMPinButtonForEligibleUser) {
  [controller() setCanChangeGPMPin:YES];

  TableViewImageItem* changeGPMPinDescription =
      static_cast<TableViewImageItem*>(
          GetTableViewItem(SectionIdentifierGooglePasswordManagerPin,
                           /*item=*/0));
  EXPECT_NSEQ(changeGPMPinDescription.title,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_TITLE));
  EXPECT_NSEQ(
      changeGPMPinDescription.detailText,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_DESCRIPTION));

  TableViewTextItem* changeGPMPinButton = static_cast<TableViewTextItem*>(
      GetTableViewItem(SectionIdentifierGooglePasswordManagerPin, /*item=*/1));
  EXPECT_NSEQ(changeGPMPinButton.text,
              l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_CHANGE_PIN));
}

TEST_F(PasswordSettingsViewControllerTest,
       CallsPresentationDelegateOnGPMPinButtonTap) {
  [controller() setCanChangeGPMPin:YES];

  id mockPresentationDelegate =
      OCMProtocolMock(@protocol(PasswordSettingsPresentationDelegate));
  controller().presentationDelegate = mockPresentationDelegate;

  OCMStub([mockPresentationDelegate showChangeGPMPinDialog]);
  NSIndexPath* pinButtonIndexPath = [NSIndexPath
      indexPathForRow:1
            inSection:GetSectionIndex(
                          SectionIdentifierGooglePasswordManagerPin)];
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:pinButtonIndexPath];
  EXPECT_OCMOCK_VERIFY(mockPresentationDelegate);
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysEncryptionOptedInForOptedInState) {
  [controller() setOnDeviceEncryptionState:
                    PasswordSettingsOnDeviceEncryptionStateOptedIn];

  TableViewImageItem* onDeviceEncryptionOptedInDescription =
      static_cast<TableViewImageItem*>(
          GetTableViewItem(SectionIdentifierOnDeviceEncryption, /*item=*/0));
  EXPECT_NSEQ(
      onDeviceEncryptionOptedInDescription.title,
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION));
  EXPECT_NSEQ(onDeviceEncryptionOptedInDescription.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_LEARN_MORE));

  TableViewTextItem* onDeviceEncryptionOptedInLearnMoreButton =
      static_cast<TableViewTextItem*>(
          GetTableViewItem(SectionIdentifierOnDeviceEncryption, /*item=*/1));
  EXPECT_NSEQ(
      onDeviceEncryptionOptedInLearnMoreButton.text,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPTED_IN_LEARN_MORE));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysEncryptionOptInButtonInOfferOptInState) {
  [controller() setOnDeviceEncryptionState:
                    PasswordSettingsOnDeviceEncryptionStateOfferOptIn];

  TableViewImageItem* onDeviceEncryptionOptInDescription =
      static_cast<TableViewImageItem*>(
          GetTableViewItem(SectionIdentifierOnDeviceEncryption, /*item=*/0));
  EXPECT_NSEQ(
      onDeviceEncryptionOptInDescription.title,
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION));
  EXPECT_NSEQ(onDeviceEncryptionOptInDescription.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPT_IN));

  TableViewTextItem* setUpOnDeviceEncryptionButton =
      static_cast<TableViewTextItem*>(
          GetTableViewItem(SectionIdentifierOnDeviceEncryption, /*item=*/1));
  EXPECT_NSEQ(setUpOnDeviceEncryptionButton.text,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_SET_UP));
}

TEST_F(PasswordSettingsViewControllerTest,
       ExportButtonDisabledWhenUserNotEligible) {
  [controller() setCanExportPasswords:NO];
  EXPECT_TRUE(GetTableViewItem(SectionIdentifierExportPasswordsButton,
                               /*item=*/0)
                  .accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordSettingsViewControllerTest,
       ExportButtonEnabledWhenUserEligible) {
  [controller() setCanExportPasswords:YES];
  EXPECT_FALSE(GetTableViewItem(SectionIdentifierExportPasswordsButton,
                                /*item=*/0)
                   .accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordSettingsViewControllerTest,
       DeleteAllDataDisabledWhenUserNotEligible) {
  [controller() setCanDeleteAllCredentials:NO];
  EXPECT_TRUE(GetTableViewItem(SectionIdentifierDeleteCredentialsButton,
                               /*item=*/0)
                  .accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordSettingsViewControllerTest,
       DeleteAllDataButtonEnabledWhenUserEligible) {
  [controller() setCanDeleteAllCredentials:YES];
  EXPECT_FALSE(GetTableViewItem(SectionIdentifierDeleteCredentialsButton,
                                /*item=*/0)
                   .accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}
