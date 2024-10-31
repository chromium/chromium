// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The expected table view section index after all the sections that are always
// displayed on top. This differs based on the addition of the automatic passkey
// upgrades toggle. Should be cleaned up after the feature is launched.
int ExpectedSectionAfterAlwaysVisibleTopSections() {
  return syncer::IsWebauthnCredentialSyncEnabled() &&
                 base::FeatureList::IsEnabled(
                     password_manager::features::kIOSPasskeysM2)
             ? 3
             : 2;
}

}  // namespace

class PasswordSettingsViewControllerTest : public PlatformTest {
 protected:
  PasswordSettingsViewControllerTest() { CreateController(); }

  TableViewItem* GetTableViewItem(int section, int item) {
    return [[controller_ tableViewModel]
        itemAtIndexPath:[NSIndexPath indexPathForItem:item inSection:section]];
  }

  void CreateController() {
    controller_ = [[PasswordSettingsViewController alloc] init];

    // Accessing this property will force the table view to be built, making
    // sure it is populated when the tests run.
    [controller_ view];
  }

  PasswordSettingsViewController* controller() { return controller_; }

 private:
  PasswordSettingsViewController* controller_;
};

TEST_F(PasswordSettingsViewControllerTest, DisplaysOfferToSavePasswords) {
  TableViewSwitchItem* savePasswordsItem = static_cast<TableViewSwitchItem*>(
      GetTableViewItem(/*section=*/0, /*item=*/0));
  EXPECT_NSEQ(savePasswordsItem.text,
              l10n_util::GetNSString(IDS_IOS_OFFER_TO_SAVE_PASSWORDS));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysOfferToSavePasswordsManagedByPolicy) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setManagedByPolicy:YES];
  TableViewInfoButtonItem* managedSavePasswordsItem =
      static_cast<TableViewInfoButtonItem*>(GetTableViewItem(/*section=*/0, 0));
  EXPECT_NSEQ(managedSavePasswordsItem.text,
              l10n_util::GetNSString(IDS_IOS_OFFER_TO_SAVE_PASSWORDS));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysMovePasswordsToAccountButtonWithLocalPasswords) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setLocalPasswordsCount:2 withUserEligibility:YES];

  TableViewImageItem* movePasswordsToAccountDescriptionItem =
      static_cast<TableViewImageItem*>(
          GetTableViewItem(/*section=*/1, /*item=*/0));
  EXPECT_NSEQ(
      movePasswordsToAccountDescriptionItem.title,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_TITLE));

  TableViewTextItem* movePasswordsToAccountButtonItem =
      static_cast<TableViewTextItem*>(
          GetTableViewItem(/*section=*/1, /*item=*/1));
  EXPECT_NSEQ(
      movePasswordsToAccountButtonItem.text,
      l10n_util::GetPluralNSStringF(
          IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_BUTTON, 2));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysPasswordInOtherAppsDisabled) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setPasswordsInOtherAppsEnabled:NO];

  TableViewDetailIconItem* passwordsInOtherAppsItem =
      static_cast<TableViewDetailIconItem*>(
          GetTableViewItem(/*section=*/1, /*item=*/0));
  EXPECT_NSEQ(passwordsInOtherAppsItem.text,
              l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS));
  EXPECT_NSEQ(passwordsInOtherAppsItem.detailText,
              l10n_util::GetNSString(IDS_IOS_SETTING_OFF));
}

TEST_F(PasswordSettingsViewControllerTest, DisplaysPasswordInOtherAppsEnabled) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setPasswordsInOtherAppsEnabled:YES];

  TableViewDetailIconItem* passwordsInOtherAppsItem =
      static_cast<TableViewDetailIconItem*>(
          GetTableViewItem(/*section=*/1, /*item=*/0));
  EXPECT_NSEQ(passwordsInOtherAppsItem.text,
              l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS));
  EXPECT_NSEQ(passwordsInOtherAppsItem.detailText,
              l10n_util::GetNSString(IDS_IOS_SETTING_ON));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysAutomaticPasskeyUpgradesSwitchWithFeatureEnabled) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kIOSPasskeysM2);

  // Re-create the controller so that the enabled flag is picked up.
  CreateController();

  TableViewSwitchItem* automaticPasskeyUpgradesSwitch =
      static_cast<TableViewSwitchItem*>(
          GetTableViewItem(/*section=*/2, /*item=*/0));
  EXPECT_NSEQ(automaticPasskeyUpgradesSwitch.text,
              l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES));
  EXPECT_NSEQ(automaticPasskeyUpgradesSwitch.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES_SUBTITLE));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysChangeGPMPinButtonForEligibleUser) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setupChangeGPMPinButton];

  TableViewImageItem* changeGPMPinDescription =
      static_cast<TableViewImageItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/0));
  EXPECT_NSEQ(changeGPMPinDescription.title,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_TITLE));
  EXPECT_NSEQ(
      changeGPMPinDescription.detailText,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_DESCRIPTION));

  TableViewTextItem* changeGPMPinButton =
      static_cast<TableViewTextItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/1));
  EXPECT_NSEQ(changeGPMPinButton.text,
              l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_CHANGE_PIN));
}

TEST_F(PasswordSettingsViewControllerTest,
       CallsPresentationDelegateOnGPMPinButtonTap) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setupChangeGPMPinButton];

  id mockPresentationDelegate =
      OCMProtocolMock(@protocol(PasswordSettingsPresentationDelegate));
  controller().presentationDelegate = mockPresentationDelegate;

  OCMStub([mockPresentationDelegate showChangeGPMPinDialog]);
  NSIndexPath* pinButtonIndexPath = [NSIndexPath
      indexPathForRow:1
            inSection:ExpectedSectionAfterAlwaysVisibleTopSections()];
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:pinButtonIndexPath];
  EXPECT_OCMOCK_VERIFY(mockPresentationDelegate);
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysEncryptionOptedInForOptedInState) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setOnDeviceEncryptionState:
                PasswordSettingsOnDeviceEncryptionStateOptedIn];

  TableViewImageItem* onDeviceEncryptionOptedInDescription =
      static_cast<TableViewImageItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/0));
  EXPECT_NSEQ(
      onDeviceEncryptionOptedInDescription.title,
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION));
  EXPECT_NSEQ(onDeviceEncryptionOptedInDescription.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_LEARN_MORE));

  TableViewTextItem* onDeviceEncryptionOptedInLearnMoreButton =
      static_cast<TableViewTextItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/1));
  EXPECT_NSEQ(
      onDeviceEncryptionOptedInLearnMoreButton.text,
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPTED_IN_LEARN_MORE));
}

TEST_F(PasswordSettingsViewControllerTest,
       DisplaysEncryptionOptInButtonInOfferOptInState) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setOnDeviceEncryptionState:
                PasswordSettingsOnDeviceEncryptionStateOfferOptIn];

  TableViewImageItem* onDeviceEncryptionOptInDescription =
      static_cast<TableViewImageItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/0));
  EXPECT_NSEQ(
      onDeviceEncryptionOptInDescription.title,
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION));
  EXPECT_NSEQ(onDeviceEncryptionOptInDescription.detailText,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPT_IN));

  TableViewTextItem* setUpOnDeviceEncryptionButton =
      static_cast<TableViewTextItem*>(GetTableViewItem(
          ExpectedSectionAfterAlwaysVisibleTopSections(), /*item=*/1));
  EXPECT_NSEQ(setUpOnDeviceEncryptionButton.text,
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_SET_UP));
}

TEST_F(PasswordSettingsViewControllerTest,
       ExportButtonDisabledWhenUserNotEligible) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setCanExportPasswords:NO];
  [consumer updateExportPasswordsButton];
  EXPECT_TRUE(GetTableViewItem(ExpectedSectionAfterAlwaysVisibleTopSections(),
                               /*item=*/0)
                  .accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordSettingsViewControllerTest,
       ExportButtonEnabledWhenUserEligible) {
  id<PasswordSettingsConsumer> consumer =
      base::apple::ObjCCast<PasswordSettingsViewController>(controller());
  [consumer setCanExportPasswords:YES];
  [consumer updateExportPasswordsButton];
  EXPECT_FALSE(GetTableViewItem(ExpectedSectionAfterAlwaysVisibleTopSections(),
                                /*item=*/0)
                   .accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}
