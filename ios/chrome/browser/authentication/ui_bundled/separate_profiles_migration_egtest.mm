// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/flat_set.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/authentication/test/separate_profiles_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

@interface SeparateProfilesMigrationTestCase : ChromeTestCase
@end

@implementation SeparateProfilesMigrationTestCase

- (void)setUp {
  [super setUp];
  ClearHistorySyncPrefs();
  // Reset the force migration timestamp pref.
  [ChromeEarlGrey
           setTimeValue:base::Time()
      forLocalStatePref:prefs::kWaitingForMultiProfileForcedMigrationTimestamp];
}

- (void)tearDownHelper {
  // Reset the force migration timestamp pref.
  [ChromeEarlGrey
           setTimeValue:base::Time()
      forLocalStatePref:prefs::kWaitingForMultiProfileForcedMigrationTimestamp];
  ClearHistorySyncPrefs();
  // Make sure any pending prefs changes are written to disk.
  [ChromeEarlGrey commitPendingUserPrefsWrite];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // The multi-profile features are initially *dis*abled for migration tests -
  // they'll be enabled later on.
  config.features_disabled.push_back(kSeparateProfilesForManagedAccounts);

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

- (void)testMigrateWithConsumerPrimaryAccount {
  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *personal* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account was moved into a separate profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 1u,
                    @"Post-migration, only the personal account should be in "
                    @"the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
  }
}

// TODO(crbug.com/433320893): Re-enable this test.
- (void)DISABLED_testMigrateWithManagedPrimaryAccount {
  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *managed* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:managedIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // After signout, the managed account should be moved into a separate profile.
  [SigninEarlGreyUI signOutWithClearDataConfirmation:YES];
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 1u,
                    @"After signout, only the personal account should remain "
                    @"in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
  }
}

- (void)testForceMigrationPrefSetForManagedPrimaryAccount {
  // Reset `kWaitingForMultiProfileForcedMigrationTimestamp`.
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kWaitingForMultiProfileForcedMigrationTimestamp];

  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *managed* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:managedIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is not set.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is set.
  GREYAssertNotEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should be set");

  base::Time lastMultiProfileForcedMigrationTimestamp = [ChromeEarlGrey
      localStateTimePref:prefs::
                             kWaitingForMultiProfileForcedMigrationTimestamp];

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` has the same
  // value.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      lastMultiProfileForcedMigrationTimestamp,
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not change");
}

- (void)testForceMigrationPrefNotSetForConsumerPrimaryAccount {
  // Reset `kWaitingForMultiProfileForcedMigrationTimestamp`.
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kWaitingForMultiProfileForcedMigrationTimestamp];

  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *personal* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is not set.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");
}

- (void)testForceMigrationPrefClearedWhenFeatureIsDisabled {
  // Reset `kWaitingForMultiProfileForcedMigrationTimestamp`.
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kWaitingForMultiProfileForcedMigrationTimestamp];

  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *managed* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:managedIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is not set.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is set.
  GREYAssertNotEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should be set");

  // Relaunch with the multi-profile features disabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kSeparateProfilesForManagedAccounts}];

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is cleared.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");
}

- (void)testForceMigration {
  // Reset `kWaitingForMultiProfileForcedMigrationTimestamp`.
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kWaitingForMultiProfileForcedMigrationTimestamp];

  // A personal and a managed identity exist on the device.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  // The *managed* identity is the primary one.
  [SigninEarlGreyUI signinWithFakeIdentity:managedIdentity];

  // Check preconditions: Both accounts exist in the profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
  }

  NSString* originalPersonalProfile = [ChromeEarlGrey currentProfileName];

  // Relaunch with the multi-profile features enabled.
  [self
      relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
             enabledFeatures:{kSeparateProfilesForManagedAccounts,
                              kSeparateProfilesForManagedAccountsForceMigration}
            disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(accountsInProfile.size(), 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert(accountsInProfile.contains(personalIdentity.gaiaId),
               @"Personal account should match");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
    GREYAssert([[ChromeEarlGrey currentProfileName]
                   isEqualToString:originalPersonalProfile],
               @"Should be in the personal profile.");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is set.
  GREYAssertNotEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should be set");

  // Relaunch again to trigger the migration.
  [ChromeEarlGrey
           setTimeValue:base::Time::Now() - base::Days(90)
      forLocalStatePref:prefs::kWaitingForMultiProfileForcedMigrationTimestamp];

  // Relaunch with the multi-profile features enabled.
  [self
      relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
             enabledFeatures:{kSeparateProfilesForManagedAccounts,
                              kSeparateProfilesForManagedAccountsForceMigration}
            disabledFeatures:{}];

  // Verify that the managed account is now in the converted-to-managed personal
  // profile.
  {
    const base::flat_set<GaiaId> accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        accountsInProfile.size(), 1u,
        @"Post-migration, the personal account should be in a new personal "
        @"profile, only the managed account is in the current managed profile");
    GREYAssert(accountsInProfile.contains(managedIdentity.gaiaId),
               @"Managed account should match");
    GREYAssert([[ChromeEarlGrey currentProfileName]
                   isEqualToString:originalPersonalProfile],
               @"The old personal profile should be the current managed one.");
    GREYAssert(
        ![[ChromeEarlGrey personalProfileName]
            isEqualToString:originalPersonalProfile],
        @"A new personal profile should have been created with a new name.");
  }

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is cleared.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");

  // Verify the enterprise onboarding UI shows to notify the user about the
  // force-migration.
  WaitForEnterpriseOnboardingScreen();
  // Verify the primary button string is the right one.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_ENTERPRISE_PROFILE_CREATION_GOTIT))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];

  // Relaunch again and verify the onboarding UI shows only once; does not show
  // again.
  [self
      relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
             enabledFeatures:{kSeparateProfilesForManagedAccounts,
                              kSeparateProfilesForManagedAccountsForceMigration}
            disabledFeatures:{}];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kManagedProfileCreationScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

@end
