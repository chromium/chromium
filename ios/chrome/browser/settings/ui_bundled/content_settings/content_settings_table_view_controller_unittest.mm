// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/content_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class ContentSettingsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  ContentSettingsTableViewControllerTest() {
    scoped_variations_service_.Get()->OverrideStoredPermanentCountry("us");
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<ContentSettingsTableViewController>(
        controller()) settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    HostContentSettingsMap* settingsMap =
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
    MailtoHandlerService* mailtoHandlerService =
        MailtoHandlerServiceFactory::GetForProfile(profile_.get());
    return [[ContentSettingsTableViewController alloc]
        initWithHostContentSettingsMap:settingsMap
                  mailtoHandlerService:mailtoHandlerService
                           prefService:profile_->GetPrefs()];
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  IOSChromeScopedTestingVariationsService scoped_variations_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that the number of items in Content Settings is correct.
TEST_F(ContentSettingsTableViewControllerTest,
       TestModelWithLanguageSettingsUI) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_CONTENT_SETTINGS_TITLE);

  if (web::features::IsWebInspectorSupportEnabled()) {
    if (IsReaderModeContentSettingsForLinkEnabled()) {
      ASSERT_EQ(3, NumberOfSections());
    } else {
      ASSERT_EQ(2, NumberOfSections());
    }
    ASSERT_EQ(1, NumberOfItemsInSection(1));
  } else {
    if (IsReaderModeContentSettingsForLinkEnabled()) {
      ASSERT_EQ(2, NumberOfSections());
    } else {
      ASSERT_EQ(1, NumberOfSections());
    }
  }
  if (base::FeatureList::IsEnabled(web::features::kEnableMeasurements)) {
    ASSERT_EQ(7, NumberOfItemsInSection(0));
  } else {
    ASSERT_EQ(6, NumberOfItemsInSection(0));
  }
  CheckDetailItemTextWithIds(IDS_IOS_BLOCK_POPUPS, IDS_IOS_SETTING_ON, 0, 0);
}

// Tests that changing a switch pref in Content Settings reconfigures the cell
// and updates its accessibility value.
TEST_F(ContentSettingsTableViewControllerTest,
       SwitchPrefUpdatesCellAccessibilityValue) {
  CreateController();
  CheckController();

  ScopedKeyWindow scoped_key_window;
  [scoped_key_window.Get() setRootViewController:controller()];
  [controller().tableView layoutIfNeeded];

  TableViewModel* model = [controller() tableViewModel];

  // 1. Verify Mini Map toggle.
  TableViewItem* miniMapItem = nil;
  for (int i = 0; i < NumberOfItemsInSection(0); ++i) {
    TableViewItem* item = GetTableViewItem(0, i);
    if ([item.accessibilityIdentifier
            isEqualToString:kSettingsMiniMapNativeCellId]) {
      miniMapItem = item;
      break;
    }
  }
  ASSERT_TRUE(miniMapItem);
  NSIndexPath* miniMapIndexPath = [model indexPathForItem:miniMapItem];
  UITableViewCell* miniMapCell =
      [controller().tableView cellForRowAtIndexPath:miniMapIndexPath];
  ASSERT_TRUE(miniMapCell);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              miniMapCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap, false);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              miniMapCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kIosMiniMapShowNativeMap, true);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              miniMapCell.accessibilityValue);

  // 2. Verify Link Preview toggle.
  TableViewItem* linkPreviewItem = nil;
  for (int i = 0; i < NumberOfItemsInSection(0); ++i) {
    TableViewItem* item = GetTableViewItem(0, i);
    if ([item.accessibilityIdentifier
            isEqualToString:kSettingsShowLinkPreviewCellId]) {
      linkPreviewItem = item;
      break;
    }
  }
  ASSERT_TRUE(linkPreviewItem);
  NSIndexPath* linkPreviewIndexPath = [model indexPathForItem:linkPreviewItem];
  UITableViewCell* linkPreviewCell =
      [controller().tableView cellForRowAtIndexPath:linkPreviewIndexPath];
  ASSERT_TRUE(linkPreviewCell);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              linkPreviewCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kLinkPreviewEnabled, false);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              linkPreviewCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kLinkPreviewEnabled, true);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              linkPreviewCell.accessibilityValue);

  // 3. Verify Detect Addresses toggle.
  TableViewItem* detectAddressesItem = nil;
  for (int i = 0; i < NumberOfItemsInSection(0); ++i) {
    TableViewItem* item = GetTableViewItem(0, i);
    if ([item.accessibilityIdentifier
            isEqualToString:kSettingsDetectAddressesCellId]) {
      detectAddressesItem = item;
      break;
    }
  }
  ASSERT_TRUE(detectAddressesItem);
  NSIndexPath* detectAddressesIndexPath =
      [model indexPathForItem:detectAddressesItem];
  UITableViewCell* detectAddressesCell =
      [controller().tableView cellForRowAtIndexPath:detectAddressesIndexPath];
  ASSERT_TRUE(detectAddressesCell);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              detectAddressesCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kDetectAddressesEnabled, false);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              detectAddressesCell.accessibilityValue);
  prefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              detectAddressesCell.accessibilityValue);
}

// Tests that toggling the UISwitch directly (e.g. via VoiceOver tap)
// synchronously updates the cell's accessibilityValue and updates the backing
// preference.
TEST_F(ContentSettingsTableViewControllerTest,
       SwitchToggleUpdatesCellAccessibilityValue) {
  CreateController();
  CheckController();

  ScopedKeyWindow scoped_key_window;
  [scoped_key_window.Get() setRootViewController:controller()];
  [controller().tableView layoutIfNeeded];

  TableViewModel* model = [controller() tableViewModel];

  TableViewItem* miniMapItem = nil;
  for (int i = 0; i < NumberOfItemsInSection(0); ++i) {
    TableViewItem* item = GetTableViewItem(0, i);
    if ([item.accessibilityIdentifier
            isEqualToString:kSettingsMiniMapNativeCellId]) {
      miniMapItem = item;
      break;
    }
  }
  ASSERT_TRUE(miniMapItem);
  NSIndexPath* miniMapIndexPath = [model indexPathForItem:miniMapItem];
  UITableViewCell* miniMapCell =
      [controller().tableView cellForRowAtIndexPath:miniMapIndexPath];
  ASSERT_TRUE(miniMapCell);

  auto find_switch = [](UIView* view, auto& self_ref) -> UISwitch* {
    if ([view isKindOfClass:[UISwitch class]]) {
      return static_cast<UISwitch*>(view);
    }
    for (UIView* subview in view.subviews) {
      UISwitch* result = self_ref(subview, self_ref);
      if (result) {
        return result;
      }
    }
    return nil;
  };

  UISwitch* switchView = find_switch(miniMapCell, find_switch);
  ASSERT_TRUE(switchView);
  EXPECT_TRUE(switchView.on);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              miniMapCell.accessibilityValue);

  // Simulate VoiceOver / user toggling switch off.
  switchView.on = NO;
  [switchView sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              miniMapCell.accessibilityValue);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kIosMiniMapShowNativeMap));

  // Simulate VoiceOver / user toggling switch on.
  switchView.on = YES;
  [switchView sendActionsForControlEvents:UIControlEventValueChanged];

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              miniMapCell.accessibilityValue);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kIosMiniMapShowNativeMap));
}

}  // namespace
