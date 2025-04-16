// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/separate_profiles_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface SeparateProfilesRollbackTestCase : ChromeTestCase
@end

@implementation SeparateProfilesRollbackTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // The multi-profile features are initially enabled.
  config.features_enabled.push_back(kIdentityDiscAccountMenu);
  config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);

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

- (void)testRollbackWithoutManagedAccounts {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should initially be enabled");

  // A personal identity exists and is the primary one.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Relaunch with the multi-profile features disabled.
  [self relaunchWithIdentities:@[ personalIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // After the relaunch, the separate-profiles feature should be disabled.
  GREYAssert(![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should be disabled now");
}

- (void)testRollbackWithoutManagedProfiles {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should initially be enabled");

  // A personal and a managed identity exist; the personal one is the primary
  // one.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Relaunch with the multi-profile features disabled.
  [self relaunchWithIdentities:@[ personalIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // After the relaunch, the separate-profiles feature should be disabled: Even
  // though there is a managed account, the corresponding managed profile was
  // never initialized.
  GREYAssert(![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should be disabled now");
}

// TODO(crbug.com/411035267): Fix this flaky test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testRollbackWithManagedProfile \
  FLAKY_testRollbackWithManagedProfile
#else
#define MAYBE_testRollbackWithManagedProfile testRollbackWithManagedProfile
#endif
- (void)MAYBE_testRollbackWithManagedProfile {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should initially be enabled");

  // A personal and a managed identity exist; the personal one is the primary
  // one.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account (and profile).
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  // Confirm the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];
  // Verify we're now on the NTP again.
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // We should now be signed in, in a new managed profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  NSString* managedProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:managedProfileName],
             @"Profile name should be unchanged");

  // Relaunch with the multi-profile features disabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // After the relaunch, the separate-profiles feature should still be enabled
  // (even though the feature flag is off now), so that the managed profile
  // remains accessible.
  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should still be enabled");

  // We should still be in the managed profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:managedProfileName],
      @"Should still be in the managed profile");

  // Switch back to the personal profile.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Should be in the personal profile again");

  // And switch to the managed profile once more.
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:managedProfileName],
      @"Should be in the managed profile again");
}

// TODO(crbug.com/411035267): Fix this flaky test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testRollbackWithManagedProfile_ManagedAccountRemoved \
  FLAKY_testRollbackWithManagedProfile_ManagedAccountRemoved
#else
#define MAYBE_testRollbackWithManagedProfile_ManagedAccountRemoved \
  testRollbackWithManagedProfile_ManagedAccountRemoved
#endif
- (void)MAYBE_testRollbackWithManagedProfile_ManagedAccountRemoved {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should initially be enabled");

  // A personal and a managed identity exist; the personal one is the primary
  // one.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account (and profile).
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  // Confirm the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];
  // Verify we're now on the NTP again.
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // We should now be signed in, in a new managed profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  NSString* managedProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:managedProfileName],
             @"Profile name should be unchanged");

  // Relaunch with the multi-profile features disabled.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // After the relaunch, the separate-profiles feature should still be enabled
  // (even though the feature flag is off now), so that the managed profile
  // remains accessible.
  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should still be enabled");

  // We should still be in the managed profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:managedProfileName],
      @"Should still be in the managed profile");

  // Remove the managed account from the device. This should cause a switch back
  // to the personal profile.
  [SigninEarlGrey forgetFakeIdentity:managedIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched to personal");

  // For now, the separate-profiles feature should still be considered enabled,
  // since this mustn't change at runtime.
  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should still be enabled");

  // Relaunch the browser once again.
  [self relaunchWithIdentities:@[ personalIdentity ]
               enabledFeatures:{}
              disabledFeatures:{kIdentityDiscAccountMenu,
                                kSeparateProfilesForManagedAccounts}];

  // Finally, the separate-profiles feature should be disabled again, since
  // there are no more managed accounts or profiles around.
  GREYAssert(![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should still be enabled");
}

// TODO(crbug.com/411035267): Fix this flaky test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testRollbackWithManagedProfile_KillSwitch \
  FLAKY_testRollbackWithManagedProfile_KillSwitch
#else
#define MAYBE_testRollbackWithManagedProfile_KillSwitch \
  testRollbackWithManagedProfile_KillSwitch
#endif
- (void)MAYBE_testRollbackWithManagedProfile_KillSwitch {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }

  NSString* personalProfileName = [ChromeEarlGrey currentProfileName];

  GREYAssert([SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should initially be enabled");

  // A personal and a managed identity exist; the personal one is the primary
  // one.
  FakeSystemIdentity* const personalIdentity =
      [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* const managedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  [SigninEarlGrey addFakeIdentity:personalIdentity];
  [SigninEarlGrey addFakeIdentity:managedIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:personalIdentity];

  // Switch to the managed account (and profile).
  OpenAccountMenu();
  [[EarlGrey
      selectElementWithMatcher:AccountMenuSecondaryAccountsButtonMatcher()]
      performAction:grey_tap()];
  // Confirm the enterprise onboarding screen.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ManagedProfileCreationScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];
  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];
  // Verify we're now on the NTP again.
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // We should now be signed in, in a new managed profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:managedIdentity];
  NSString* managedProfileName = [ChromeEarlGrey currentProfileName];
  GREYAssert(![personalProfileName isEqualToString:managedProfileName],
             @"Should have switched to a different profile");

  // Relaunch with the multi-profile killswitch.
  [self relaunchWithIdentities:@[ personalIdentity, managedIdentity ]
               enabledFeatures:{kSeparateProfilesForManagedAccountsKillSwitch}
              disabledFeatures:{}];

  // The feature should be disabled now, even though there is (or was) a
  // managed profile.
  GREYAssert(![SigninEarlGrey areSeparateProfilesForManagedAccountsEnabled],
             @"Separate profiles should be disabled");

  // The browser should have automatically switched back to the personal
  // profile.
  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Should have switched back to the personal profile");
}

@end
