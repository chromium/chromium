// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/policy/policy_constants.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Sign in and sync using a fake identity.
void SignInAndSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Opens tabs settings.
void OpenTabsSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::TabsSettingsButton()];
}

// Opens tab pickup settings from the tabs settings.
void OpenTabPickupFromTabsSettings() {
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::TabPickupSettingsButton()];
}

// GREYMatcher for the tab pickup settings table view.
id<GREYMatcher> SettingsTabPickupTableView() {
  return grey_accessibilityID(kTabPickupSettingsTableViewId);
}

// GREYMatcher for the detail text of the tab pickup item in the tabs settings
// screen.
id<GREYMatcher> TabsSettingsTabPickupDetailText(bool enabled) {
  NSString* detail_text = l10n_util::GetNSString(enabled ? IDS_IOS_SETTING_ON
                                                         : IDS_IOS_SETTING_OFF);

  return grey_allOf(grey_accessibilityID(kSettingsTabPickupCellId),
                    grey_descendant(grey_text(detail_text)), nil);
}

// GREYMatcher for the tab pickup switch item in the tab pickup settings screen.
id<GREYMatcher> TabPickupSettingsSwitchItem(bool is_toggled_on, bool enabled) {
  return chrome_test_util::TableViewSwitchCell(kTabPickupSettingsSwitchItemId,
                                               is_toggled_on, enabled);
}

}  // namespace

@interface TabPickupSettingsTestCase : ChromeTestCase
@end

@implementation TabPickupSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kTabPickupThreshold);

  if ([self isRunningTest:@selector(testTabPickupSettingsSynced)]) {
    // With kReplaceSyncPromosWithSignInPromos enabled, Sync can't be enabled
    // anymore.
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  return config;
}

- (void)setUp {
  [super setUp];
}

// Ensures that the tab pickup settings are correctly working when synced.
- (void)testTabPickupSettingsSynced {
  SignInAndSync();

  OpenTabsSettings();
  [[EarlGrey selectElementWithMatcher:TabsSettingsTabPickupDetailText(true)]
      assertWithMatcher:grey_sufficientlyVisible()];

  OpenTabPickupFromTabsSettings();
  [[EarlGrey selectElementWithMatcher:SettingsTabPickupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/true, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Ensures that the tab pickup settings are correctly working when not synced.
- (void)testTabPickupSettingsNotSynced {
  OpenTabsSettings();
  [[EarlGrey selectElementWithMatcher:TabsSettingsTabPickupDetailText(false)]
      assertWithMatcher:grey_sufficientlyVisible()];

  OpenTabPickupFromTabsSettings();
  [[EarlGrey selectElementWithMatcher:SettingsTabPickupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/false, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Ensures that the tab pickup settings are correctly working when sign in from
// tab pickup settings.
- (void)testTabPickupSettingsSignInFlow {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  OpenTabsSettings();
  [[EarlGrey selectElementWithMatcher:TabsSettingsTabPickupDetailText(false)]
      assertWithMatcher:grey_sufficientlyVisible()];

  OpenTabPickupFromTabsSettings();
  [[EarlGrey selectElementWithMatcher:SettingsTabPickupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toogle and skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/false, /*enabled=*/true)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/true, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];

  // Toogle and Sign-in.
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/false, /*enabled=*/true)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/true, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/true, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that for a signed in user, after declining twice History Sync, the
// History Sync is still shown when tapping on the tab switcher item.
- (void)testTabPickupSettingsDelineRepeatedlyHistorySyncIfSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenTabsSettings();
  [[EarlGrey selectElementWithMatcher:TabsSettingsTabPickupDetailText(false)]
      assertWithMatcher:grey_sufficientlyVisible()];

  OpenTabPickupFromTabsSettings();
  [[EarlGrey selectElementWithMatcher:SettingsTabPickupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on switch item and decline History Sync 3 times.
  for (int i = 0; i <= 2; i++) {
    // Toogle and Sign-in.
    [[EarlGrey
        selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                     /*is_toggled_on=*/false, /*enabled=*/true)]
        performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

    // Verify that the History Sync Opt-In screen is shown.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kHistorySyncViewAccessibilityIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Decline History Sync.
    [[[EarlGrey selectElementWithMatcher:
                    chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
           usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
        onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
        performAction:grey_tap()];
    [ChromeEarlGrey
        waitForUIElementToDisappearWithMatcher:
            grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  }

  // Toogle and Sign-in.
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/false, /*enabled=*/true)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TabPickupSettingsSwitchItem(
                                   /*is_toggled_on=*/true, /*enabled=*/true)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Ensures that the tab pickup settings are correctly working when sign-in is
// disbled by an enterprise policy.
- (void)testTabPickupSettingsSignInDisabledByPolicy {
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  OpenTabsSettings();
  [[EarlGrey selectElementWithMatcher:TabsSettingsTabPickupDetailText(false)]
      assertWithMatcher:grey_sufficientlyVisible()];

  OpenTabPickupFromTabsSettings();
  [[EarlGrey selectElementWithMatcher:SettingsTabPickupTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the managed item is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabPickupSettingsManagedItemId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
