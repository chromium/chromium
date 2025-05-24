// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/separate_profiles_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_matchers.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

id<GREYMatcher> ManagedProfileCreationBrowsingDataButtonMatcher() {
  return grey_accessibilityID(kBrowsingDataButtonAccessibilityIdentifier);
}

id<GREYMatcher> SeparateBrowsingDataCellMatcher() {
  return grey_accessibilityID(kKeepBrowsingDataSeparateCellId);
}

id<GREYMatcher> MergeBrowsingDataCellMatcher() {
  return grey_accessibilityID(kMergeBrowsingDataCellId);
}

id<GREYMatcher> ManagedProfileCreationSubtitleMergeByDefaultMatcher() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DESCRIPTION));
}

id<GREYMatcher> ManagedProfileCreationSubtitleMatcher() {
  return grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_SUBTITLE));
}

id<GREYMatcher> ManagedProfileCreationDataMigrationDisabledSubtitleMatcher() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_KEEP_BROWSING_DATA_DISABLED_DESCRIPTION));
}

}  // namespace

@interface SeparateProfilesTestCase : ChromeTestCase
@end

@implementation SeparateProfilesTestCase

+ (void)setUpForTestCase {
  [SigninEarlGrey setUseFakeResponsesForProfileSeparationPolicyRequests];
}

+ (void)tearDown {
  [SigninEarlGrey clearUseFakeResponsesForProfileSeparationPolicyRequests];
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  ClearHistorySyncPrefs();
}

- (void)tearDownHelper {
  ClearHistorySyncPrefs();
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);
  config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);

  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  return config;
}

- (void)relaunchWithIdentities:(NSArray<FakeSystemIdentity*>*)identities
               enabledFeatures:
                   (const std::vector<base::test::FeatureRef>&)enabled
              disabledFeatures:
                  (const std::vector<base::test::FeatureRef>&)disabled {
  // Before restarting, make sure any pending prefs changes are written to disk.
  [ChromeEarlGrey commitPendingUserPrefsWrite];

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled = enabled;
  config.features_disabled = disabled;
  config.additional_args.push_back(
      base::StrCat({"-", test_switches::kAddFakeIdentitiesAtStartup, "=",
                    [FakeSystemIdentity encodeIdentitiesToBase64:identities]}));
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Tests that signing in from a signed out state with a managed account
// shows the enterprise onboarding only the first time and that by default
// existing browsing data is kept separate from the managed profile.
- (void)testSigninWithManagedAccountFromUnsignedStateSeparateData {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // Verifies that the subtitle is the right one.
  [[EarlGrey selectElementWithMatcher:ManagedProfileCreationSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the browsing data management screen.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the browsing data management screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      BrowsingDataManagementScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:BrowsingDataManagementScreenMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Browsing data kept separate by default.
  [[EarlGrey selectElementWithMatcher:SeparateBrowsingDataCellMatcher()]
      assertWithMatcher:grey_selected()];
  // Close the browsing data management screen by going back.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ManagedProfileCreationNavigationBarBackButton()]
      performAction:grey_tap()];

  // Wait for the browsing data management screen to disappear, and the
  // enteprise onboarding screen to appear again.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // We should be in a new managed profile.
  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![originalProfileName isEqualToString:newProfileName],
             @"Profile name should be changed");

  // Sign out - that should cause a switch back to the personal profile.
  SignoutFromAccountMenu();
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([personalProfileName isEqualToString:originalProfileName],
             @"Profile name should be the personal one");

  // Sign in again.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for the profile to finish loading again.
  // TODO(crbug.com/399033938): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  // The user should be signed in without having to see the managed profile
  // onboarding a second time.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests that signing in from a signed out state with a managed account
// shows the enterprise onboarding only the first time. And if the user
// decides to keep their existing data into the managed profile, the existing
// profile is converted.
- (void)testSigninWithManagedAccountFromUnsignedStateConvertsProfile {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];
  // Verify that the subtitle is the right one.
  [[EarlGrey selectElementWithMatcher:ManagedProfileCreationSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the browsing data management screen.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      BrowsingDataManagementScreenMatcher()];

  [[EarlGrey selectElementWithMatcher:SeparateBrowsingDataCellMatcher()]
      assertWithMatcher:grey_selected()];
  // Select merging browsing data to convert the current profile into a managed
  // one.
  [[EarlGrey selectElementWithMatcher:MergeBrowsingDataCellMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:MergeBrowsingDataCellMatcher()]
      assertWithMatcher:grey_selected()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ManagedProfileCreationNavigationBarBackButton()]
      performAction:grey_tap()];

  // Wait for the browsing data management screen to disappear, and the
  // enteprise onboarding screen to appear again.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // We should still be in the same profile, now converted to be a managed
  // profile.
  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName isEqualToString:newProfileName],
             @"Profile name should be unchanged");

  // Sign out - this should cause a switch back to the personal profile.
  SignoutFromAccountMenu();
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:newProfileName],
             @"Profile name should be the personal one");

  // Sign in again
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for the profile to finish loading again.
  // TODO(crbug.com/399033938): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  // The user should be signed in without having to see the managed profile
  // onboarding a second time.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests that signing in from a signed out state with a managed account shows
// the enterprise onboarding only the first time. And the user cannot merge
// existing browsing data because it is disabled by policy.
- (void)testSigninWithManagedAccountFromUnsignedStateWithDataMigrationDisabled {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Browsing data migration is disabled by policy.
  [ChromeEarlGrey
      setIntegerValue:policy::ALWAYS_SEPARATE
          forUserPref:prefs::kProfileSeparationDataMigrationSettings];
  GREYAssertEqual(
      policy::ALWAYS_SEPARATE,
      [ChromeEarlGrey
          userIntegerPref:prefs::kProfileSeparationDataMigrationSettings],
      @"Profile separation data migration settings not properly set.");
  // It's enabled on account level, but the strictest value should apply.
  [SigninEarlGrey setPolicyResponseForNextProfileSeparationPolicyRequest:
                      policy::USER_OPT_IN];

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // The data migration disabled message should be shown.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationDataMigrationDisabledSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // We should be in a new managed profile.
  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![originalProfileName isEqualToString:newProfileName],
             @"Profile name should be unchanged");

  // Sign out - this should cause a switch back to the personal profile.
  SignoutFromAccountMenu();
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([personalProfileName isEqualToString:originalProfileName],
             @"Profile name should be the personal one");

  // Sign in again.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // The user should be signed in without having to see the managed profile
  // onboarding a second time.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests that signing in from a signed out state with a managed account shows
// the enterprise onboarding. And the user cannot merge existing browsing data
// because it is disabled by policy.
- (void)
    testSigninWithManagedAccountFromUnsignedStateWithDataMigrationDisabledOnAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Browsing data migration is disabled by policy.
  [SigninEarlGrey setPolicyResponseForNextProfileSeparationPolicyRequest:
                      policy::ALWAYS_SEPARATE];

  // Set device policy that enables migration, the most strict value should
  // apply.
  [ChromeEarlGrey
      setIntegerValue:policy::USER_OPT_OUT
          forUserPref:prefs::kProfileSeparationDataMigrationSettings];

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // The data migration disabled message should be shown.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationDataMigrationDisabledSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests that signing in from a signed out state with a managed account
// shows the enterprise onboarding only the first time and merging browsing data
// is suggested by policy.
- (void)testSigninWithManagedAccountFromUnsignedStateWithDataMergingSuggested {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Browsing data migration is suggested by policy.
  [ChromeEarlGrey
      setIntegerValue:policy::USER_OPT_OUT
          forUserPref:prefs::kProfileSeparationDataMigrationSettings];
  GREYAssertEqual(
      policy::USER_OPT_OUT,
      [ChromeEarlGrey
          userIntegerPref:prefs::kProfileSeparationDataMigrationSettings],
      @"Profile separation data migration settings not properly set.");

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // Verifies that the subtitle is the right one.
  [[EarlGrey selectElementWithMatcher:ManagedProfileCreationSubtitleMergeByDefaultMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the browsing data management screen.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      BrowsingDataManagementScreenMatcher()];

  // Browsing data should be merged by default due to the policy.
  [[EarlGrey selectElementWithMatcher:MergeBrowsingDataCellMatcher()]
      assertWithMatcher:grey_selected()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ManagedProfileCreationNavigationBarBackButton()]
      performAction:grey_tap()];

  // Wait for the browsing data management screen to disappear.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // We should be in the same profile (personal profile got converted to
  // managed).
  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName isEqualToString:newProfileName],
             @"Profile name should be unchanged");

  // Sign out - this should cause a switch to a new personal profile.
  [SigninEarlGreyUI signOut];
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:originalProfileName],
             @"Personal profile should be different");

  // Sign in again.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // The user should be signed in without having to see the managed profile
  // onboarding a second time.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests that signing in from a signed out state with a managed account shows
// the enterprise onboarding and when merging browsing data is suggested by
// policy on the account, it does not actually suggest merging the data since
// that value is not supported at account level.
- (void)
    testSigninWithManagedAccountFromUnsignedStateWithDataMergingSuggestedOnAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  // Setup: There's 1 managed account. No account is signed in.
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // Set the policy to suggest data migration. Note that this is not actually
  // supported on the account level, so the user should still see
  // "keep separate" by default.
  [SigninEarlGrey setPolicyResponseForNextProfileSeparationPolicyRequest:
                      policy::USER_OPT_OUT];

  // Switch to the managed account, and sign in with the managed account.
  TapIdentityDisc();
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Wait for enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // Open the browsing data management screen.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      BrowsingDataManagementScreenMatcher()];

  // Browsing data not merged by default when the policy comes from the account
  // and not the device.
  [[EarlGrey selectElementWithMatcher:SeparateBrowsingDataCellMatcher()]
      assertWithMatcher:grey_selected()];

  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ManagedProfileCreationNavigationBarBackButton()]
      performAction:grey_tap()];

  // Wait for the browsing data management screen to disappear.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // We are still signed out before accepting enterprise management.
  [SigninEarlGrey verifySignedOut];

  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
}

// Tests switching to a managed account (and thus managed profile) and back via
// the account menu.
- (void)testSwitchFromPersonalToManagedAndBack {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];
  // No merge browsing data button shown.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_notVisible()];
  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the New Tab page appeared in the new profile.
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // Verify that the profile was actually switched.
  NSString* managedProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:managedProfileName],
             @"Profile should have been switched");

  // Switch back to the personal account, which triggers a switch back to the
  // personal profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the profile to finish loading again.
  // TODO(crbug.com/399033938): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Verify that the profile was actually switched back.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched");
}

// Tests switching to a managed account (and thus managed profile) and the
// managed account is removed while the enterprise onboarding is shown.
- (void)testSwitchFromPersonalToManagedAndManagedAccountRemovedFromDevice {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // Remove the managed account from device.
  [SigninEarlGrey forgetFakeIdentity:managedIdentity];

  // Ensure the enterprise onboarding screen did disapepar.
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Verify that the profile is still the personal one.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should stay the same");
}

// Tests switching to a managed account but refusing the enterprise onboarding
// screen.
- (void)testRefuseToSwitchToManageAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];

  // Wait for enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];

  // Refuse the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  GREYWaitForAppToIdle(@"App failed to idle");

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Verify that the profile was not switched.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should not have been switched");
}
- (void)testProfileNotDeletedOnRemovePersonalAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Remove `personalIdentity` from device.
  OpenManageAccountsView();
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:personalIdentity.userEmail])]
      performAction:grey_tap()];
  // Tap on `personalIdentity` confirm remove button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_REMOVE_ACCOUNT_LABEL)]
      performAction:grey_tap()];

  // Verify the current profile is still the personal profile, but account got
  // signed out.
  [SigninEarlGrey verifySignedOut];
  GREYAssert(
      [personalProfileName isEqualToString:[ChromeEarlGrey currentProfileName]],
      @"Profile should be personal");
}

- (void)testProfileDeletedOnRemoveManagedAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  // Wait for the enterprise onboarding screen.
  ConditionBlock enterpriseOnboardingCondition = ^{
    NSError* error;
    [[EarlGrey selectElementWithMatcher:ManagedProfileCreationScreenMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];

    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout,
                 enterpriseOnboardingCondition),
             @"Enterprise onboarding didn't appear.");
  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  //  Dismiss signed in snackbar.
  NSString* signedInSnackbarTitle = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16(managedIdentity.userFullName));
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(signedInSnackbarTitle)]
      performAction:grey_tap()];

  // Confirm profile switched.
  GREYAssert([[ChromeEarlGrey currentProfileName]
                 isEqualToString:[ChromeEarlGrey currentProfileName]],
             @"Profile should be personal");

  // Remove `managedIdentity` from device.
  OpenManageAccountsView();
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:managedIdentity.userEmail])]
      performAction:grey_tap()];
  // Tap on `managedIdentity` confirm remove button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_REMOVE_ACCOUNT_LABEL)]
      performAction:grey_tap()];

  // Wait for the profile switch to complete.
  // TODO(crbug.com/399033938): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  // Verify that the profile was actually switched back to personal.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched");
}

- (void)testProfileDeletedOnForgetManagedAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  // Wait for the enterprise onboarding screen.
  ConditionBlock enterpriseOnboardingCondition = ^{
    NSError* error;
    [[EarlGrey selectElementWithMatcher:ManagedProfileCreationScreenMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];

    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout,
                 enterpriseOnboardingCondition),
             @"Enterprise onboarding didn't appear.");
  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // Note: The profile switch happens here.

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Confirm profile switched.
  GREYAssert(![[ChromeEarlGrey currentProfileName]
                 isEqualToString:personalProfileName],
             @"Profile should NOT be personal");

  // Forget `managedIdentity` from device.
  [SigninEarlGrey forgetFakeIdentity:managedIdentity];

  // Verify that the profile was actually switched back to personal.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched back to personal");
}

- (void)testProfileDeletedOnManagedAccountGone {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  // Setup: There's 1 personal and 1 managed account. The personal account is
  // signed in.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];
  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  // Switch to the managed account, which triggers a switch to a new managed
  // profile.
  OpenAccountMenu();
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];
  // Wait for the enterprise onboarding screen.
  ConditionBlock enterpriseOnboardingCondition = ^{
    NSError* error;
    [[EarlGrey selectElementWithMatcher:ManagedProfileCreationScreenMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];

    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout,
                 enterpriseOnboardingCondition),
             @"Enterprise onboarding didn't appear.");
  // Confirm the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // Confirm profile switched.
  GREYAssert(![[ChromeEarlGrey currentProfileName]
                 isEqualToString:personalProfileName],
             @"Profile should NOT be personal");

  // Relaunch the browser without the managed account. This simulates the
  // situation where the managed account was removed in another Google app.
  [self relaunchWithIdentities:@[ personalIdentity ]
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the profile was switched back to personal.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched back to personal");
}

@end

@interface SeparateProfilesFRETestCase : ChromeTestCase
@end

@implementation SeparateProfilesFRETestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);
  config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);
  // Note: Can't use the actual feature definition, because its build target
  // depends on a bunch of stuff that mustn't make it into the EG test target.
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");

  // Disable AnimatedDefaultBrowserPromoInFRE because it introduces a new
  // Default Browser screen with a different ID.
  config.additional_args.push_back(
      "--disable-features=AnimatedDefaultBrowserPromoInFRE");

  // Enable the FRE.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch the app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  return config;
}

// Tests signing in with a personal account during the FRE. This shouldn't have
// any particular side-effects; it mostly exists as a base case for
// `testSignInWithManagedAccount`.
- (void)testSignInWithPersonalAccount {
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName
                 isEqualToString:[ChromeEarlGrey personalProfileName]],
             @"Profile should be personal");

  // Signin screen: Press "Continue as foo1".
  [ChromeEarlGrey waitForMatcher:SigninScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          personalIdentity)]
      performAction:grey_tap()];

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Default browser screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:DefaultBrowserScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName isEqualToString:newProfileName],
             @"Profile name should be unchanged");
  GREYAssert(
      [newProfileName isEqualToString:[ChromeEarlGrey personalProfileName]],
      @"Profile should still be personal");
}

// Tests signing in with a managed account during the FRE. This should convert
// the existing profile to a managed profile.
- (void)testSignInWithManagedAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  NSString* originalProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName
                 isEqualToString:[ChromeEarlGrey personalProfileName]],
             @"Profile should be personal");

  // Signin screen: Press "Continue as foo".
  [ChromeEarlGrey waitForMatcher:SigninScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:ContinueButtonWithIdentityMatcher(
                                          managedIdentity)]
      performAction:grey_tap()];

  // Managed profile creation/confirmation screen: Accept.
  [ChromeEarlGrey waitForMatcher:ManagedProfileCreationScreenMatcher()];
  // No merge browsing data button shown.
  [[EarlGrey selectElementWithMatcher:
                 ManagedProfileCreationBrowsingDataButtonMatcher()]
      assertWithMatcher:grey_notVisible()];
  // Confirm the screen.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Default browser screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:DefaultBrowserScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];

  // We should still be in the same profile, now converted to be a managed
  // profile.
  NSString* newProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert([originalProfileName isEqualToString:newProfileName],
             @"Profile name should be unchanged");
  GREYAssertFalse(
      [newProfileName isEqualToString:[ChromeEarlGrey personalProfileName]],
      @"Profile should NOT be personal anymore");
}

@end
