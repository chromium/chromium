// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/grit/ios_strings.h"
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

// GREYMatcher for the tab pickup privacy footer in the tab pickup settings
// screen corresponding to the given `message_id`.
id<GREYMatcher> TabPickupPrivacyFooter(int message_id) {
  NSString* message = l10n_util::GetNSString(message_id);
  return grey_allOf(
      grey_accessibilityID(kTabPickupSettingsPrivacyFooterId),
      grey_descendant(grey_text(ParseStringWithLinks(message).string)), nil);
}

// GREYMatcher for the tab pickup privacy Sync footer link.
id<GREYMatcher> TabPickupPrivacyFooterSyncLink() {
  return grey_allOf(grey_accessibilityLabel(@"Sync"),
                    grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
                    nil);
}

// GREYMatcher for the tab pickup privacy Google Servicies footer link.
id<GREYMatcher> TabPickupPrivacyFooterGoogleServicesLink() {
  return grey_allOf(grey_accessibilityLabel(@"Google Services"),
                    grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
                    nil);
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
  [[EarlGrey selectElementWithMatcher:
                 TabPickupPrivacyFooter(
                     IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER)]
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
                                   /*is_toggled_on=*/false, /*enabled=*/false)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:TabPickupPrivacyFooter(
                                   IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Ensures that tab pickup settings links are correctly working.
// TODO(crbug.com/1487979): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testTabPickupSettingsLinks DISABLED_testTabPickupSettingsLinks
#else
#define MAYBE_testTabPickupSettingsLinks testTabPickupSettingsLinks
#endif
- (void)MAYBE_testTabPickupSettingsLinks {
  SignInAndSync();

  OpenTabsSettings();
  OpenTabPickupFromTabsSettings();

  // Check the "Sync" link.
  [[EarlGrey selectElementWithMatcher:TabPickupPrivacyFooterSyncLink()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton(
                                          0)] performAction:grey_tap()];

  // Check the "Google Services" link.
  [[EarlGrey
      selectElementWithMatcher:TabPickupPrivacyFooterGoogleServicesLink()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGoogleServicesSettingsViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
