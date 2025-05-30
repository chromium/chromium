// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface SeparateProfilesMigrationTestCase : ChromeTestCase
@end

@implementation SeparateProfilesMigrationTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // The multi-profile features are initially *dis*abled for migration tests -
  // they'll be enabled later on.
  config.features_disabled.push_back(kIdentityDiscAccountMenu);
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

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
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        [accountsInProfile count], 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account was moved into a separate profile.
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 1u,
                    @"Post-migration, only the personal account should be in "
                    @"the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
  }
}

- (void)testMigrateWithManagedPrimaryAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

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
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        [accountsInProfile count], 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
               @"Managed account should match");
  }

  // After signout, the managed account should be moved into a separate profile.
  [SigninEarlGreyUI signOutWithClearDataConfirmation:YES];
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 1u,
                    @"After signout, only the personal account should remain "
                    @"in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
  }
}

- (void)testForceMigrationPrefSetForManagedPrimaryAccount {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

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
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        [accountsInProfile count], 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
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
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
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
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile.
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

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
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        [accountsInProfile count], 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
               @"Managed account should match");
  }

  // Relaunch with the multi-profile features enabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

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
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual(
        [accountsInProfile count], 2u,
        @"Pre-migration, both accounts should be in the personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
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
               enabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}
              disabledFeatures:{}];

  // Verify that the managed account remained in the personal profile, since it
  // is the primary account.
  {
    NSSet<NSString*>* accountsInProfile =
        [SigninEarlGrey accountsInProfileGaiaIDs];
    GREYAssertEqual([accountsInProfile count], 2u,
                    @"Post-migration, both accounts should still be in the "
                    @"personal profile");
    GREYAssert([accountsInProfile containsObject:personalIdentity.gaiaID],
               @"Personal account should match");
    GREYAssert([accountsInProfile containsObject:managedIdentity.gaiaID],
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
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // Verify `kWaitingForMultiProfileForcedMigrationTimestamp` is cleared.
  GREYAssertEqual(
      [ChromeEarlGrey
          localStateTimePref:
              prefs::kWaitingForMultiProfileForcedMigrationTimestamp],
      base::Time(),
      @"kWaitingForMultiProfileForcedMigrationTimestamp should not be set");
}

@end
