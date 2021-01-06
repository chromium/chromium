// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(GoogleServicesSettingsAppInterface);
#pragma clang diagnostic pop

using l10n_util::GetNSString;
using chrome_test_util::AddAccountButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SyncSettingsConfirmButton;

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : ChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scrollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Adds the command-line switch to enable support for the BrowserSignin
  // policy.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kInstallBrowserSigninHandler);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  [self openGoogleServicesSettings];

  // Assert title and accessibility.
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the Google Services settings.
- (void)testOpeningServices {
  [self openGoogleServicesSettings];
  [self assertNonPersonalizedServices];
}

// Tests that the Google Services settings reloads without crashing when the
// primary account is removed.
// Regression test for crbug.com/1033901
- (void)testRemovePrimaryAccount {
  // Signin.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];
  // Remove the primary account.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  // Assert the UI has been reloaded by testing for the signin cell being
  // visible.
  id<GREYMatcher> signinCellMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE
                      detailTextID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SIGN_IN_DETAIL_TEXT];
  [[EarlGrey selectElementWithMatcher:signinCellMatcher]
      assertWithMatcher:grey_notNil()];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the following steps:
//  + Opens sign-in from Google services
//  + Taps on the settings link to open the advanced sign-in settings
//  + Opens "Data from Chromium sync" to interrupt sign-in
- (void)testInterruptSigninFromGoogleServicesSettings {
  [GoogleServicesSettingsAppInterface
      blockAllNavigationRequestsForCurrentWebState];
  // Add default identity.
  [self setTearDownHandler:^{
    [GoogleServicesSettingsAppInterface
        unblockAllNavigationRequestsForCurrentWebState];
  }];
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];
  // Open sign-in.
  id<GREYMatcher> signinCellMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE
                      detailTextID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SIGN_IN_DETAIL_TEXT];
  [[EarlGrey selectElementWithMatcher:signinCellMatcher]
      performAction:grey_tap()];
  // Open Settings link.
  [SigninEarlGreyUI tapSettingsLink];
  // Open "Manage Sync" settings.
  id<GREYMatcher> manageSyncMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE
                      detailTextID:0];
  [[EarlGrey selectElementWithMatcher:manageSyncMatcher]
      performAction:grey_tap()];
  // Open "Data from Chrome sync".
  id<GREYMatcher> manageSyncScrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  id<GREYMatcher> dataFromChromeSyncMatcher = [self
      cellMatcherWithTitleID:IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE
                detailTextID:
                    IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION];
  [[self elementInteractionWithGreyMatcher:dataFromChromeSyncMatcher
                         scrollViewMatcher:manageSyncScrollViewMatcher]
      performAction:grey_tap()];
  // Needs to wait until the sign-in dialog is fully dismissed to continue.
  [ChromeEarlGreyUI waitForAppToIdle];
  [self openGoogleServicesSettings];
  // Verify the sync is not confirmed yet.
  [self assertCellWithTitleID:IDS_IOS_SYNC_SETUP_NOT_CONFIRMED_TITLE
                 detailTextID:IDS_IOS_SYNC_SETTINGS_NOT_CONFIRMED_DESCRIPTION];
}

// Opens the SSO add account view, from the Google services settings.
// See: crbug.com/1076843
- (void)testOpenSSOAddAccount {
  // Signin.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];
  // Open account list view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAccountListItemAccessibilityIdentifier)]
      performAction:grey_tap()];
  // Open sso add account view.
  [[EarlGrey selectElementWithMatcher:AddAccountButton()]
      performAction:grey_tap()];
  // Close it.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that the Safe Browsing toggle reflects the current value of the
// Safe Browsing preference, and updating the toggle also updates the
// preference.
- (void)testToggleSafeBrowsing {
  // Start in the default (opted-in) state for Safe Browsing.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];

  [self openGoogleServicesSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                  @"Failed to toggle-off Safe Browsing");

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Open settings again, verify Safe Browsing is still disabled, and re-enable
  // it.
  [self openGoogleServicesSettings];
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to toggle-on Safe Browsing");
}

// Tests that password leak detection can only be toggled if Safe Browsing is
// enabled.
- (void)testTogglePasswordLeakCheck {
  // Ensure that Safe Browsing and password leak detection opt-outs start in
  // their default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordLeakDetectionEnabled];

  // Sign in.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  // Open "Google Services" settings.
  [self openGoogleServicesSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check that the password leak check toggle is both toggled off and disabled.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/NO)] assertWithMatcher:grey_notNil()];

  // Toggle Safe Browsing on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kSafeBrowsingItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check that the password leak check toggle is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-off password leak checks");

  // Toggle password leak check detection back on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SettingsSwitchCell(
                 kPasswordLeakCheckItemAccessibilityIdentifier,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-on password leak checks");
}

// Tests the following steps:
//  + Opens sign-in from Google services
//  + Taps on the settings link to open the advanced sign-in settings
//  + Opens "Manage Sync" twice
// The "Manage Sync" should not be disabled when closing "Manage Sync" view.
- (void)testOpenManageSyncSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  // Open "Manage Sync" settings.
  id<GREYMatcher> manageSyncMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE
                      detailTextID:0];
  [[EarlGrey selectElementWithMatcher:manageSyncMatcher]
      performAction:grey_tap()];

  id<GREYMatcher> backButtonMatcher =
      grey_allOf(SettingsMenuBackButton(),
                 grey_descendant(grey_kindOfClass([UIImageView class])), nil);
  // Back to the Google services settings view.
  [[EarlGrey selectElementWithMatcher:backButtonMatcher]
      performAction:grey_tap()];
  // Open "Manage Sync" settings, again.
  [[EarlGrey selectElementWithMatcher:manageSyncMatcher]
      performAction:grey_tap()];
  // Back to the Google services settings view.
  [[EarlGrey selectElementWithMatcher:backButtonMatcher]
      performAction:grey_tap()];

  // Close the advance settings.
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the sign-in button can't be used when sign-in is disabled.
- (void)testSigninDisabled {
  // Disable browser sign-in.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSigninAllowed];

  // Open Google services settings and verify the sign-in cell shows the
  // "sign-in disabled" text.
  [self openGoogleServicesSettings];
  id<GREYMatcher> signinMatcher =
      [self cellMatcherWithTitleID:IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE
                      detailTextID:IDS_IOS_SETTINGS_SIGNIN_DISABLED];
  [[EarlGrey selectElementWithMatcher:signinMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Attempt to tap the sign-in cell.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:signinMatcher] performAction:grey_tap()];

  // Verify the sync view isn't showing.
  id<GREYMatcher> syncTitleMatcher = grey_allOf(
      grey_accessibilityLabel(
          GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE)),
      grey_kindOfClass([UILabel class]), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:syncTitleMatcher]
      assertWithMatcher:grey_nil()];

  // Prefs clean-up.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSigninAllowed];
}

#pragma mark - Helpers

// Opens the Google services settings.
- (void)openGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  self.scrollViewMatcher =
      grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Scrolls Google services settings to the top.
- (void)scrollUp {
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.1f, 0.1f)];
}

// Returns grey matcher for a cell with |titleID| and |detailTextID|.
- (id<GREYMatcher>)cellMatcherWithTitleID:(int)titleID
                             detailTextID:(int)detailTextID {
  NSString* accessibilityLabel = GetNSString(titleID);
  if (detailTextID) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                   GetNSString(detailTextID)];
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_kindOfClassName(@"UITableViewCell"),
                    grey_sufficientlyVisible(), nil);
}

// Returns GREYElementInteraction for |matcher|, using |scrollViewMatcher| to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                    scrollViewMatcher:(id<GREYMatcher>)scrollViewMatcher {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

// Returns GREYElementInteraction for |matcher|, with |self.scrollViewMatcher|
// to scroll.
- (GREYElementInteraction*)elementInteractionWithGreyMatcher:
    (id<GREYMatcher>)matcher {
  return [self elementInteractionWithGreyMatcher:matcher
                               scrollViewMatcher:self.scrollViewMatcher];
}

// Returns GREYElementInteraction for a cell based on the title string ID and
// the detail text string ID. |detailTextID| should be set to 0 if it doesn't
// exist in the cell.
- (GREYElementInteraction*)cellElementInteractionWithTitleID:(int)titleID
                                                detailTextID:(int)detailTextID {
  id<GREYMatcher> cellMatcher = [self cellMatcherWithTitleID:titleID
                                                detailTextID:detailTextID];
  return [self elementInteractionWithGreyMatcher:cellMatcher];
}

// Asserts that a cell exists, based on its title string ID and its detail text
// string ID. |detailTextID| should be set to 0 if it doesn't exist in the cell.
- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[self cellElementInteractionWithTitleID:titleID detailTextID:detailTextID]
      assertWithMatcher:grey_notNil()];
}

// Asserts that the non-personalized service section is visible.
- (void)assertNonPersonalizedServices {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL];
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
}

@end
