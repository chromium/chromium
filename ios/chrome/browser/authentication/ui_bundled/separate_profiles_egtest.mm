// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_matchers.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Matcher for the identity disc.
id<GREYMatcher> IdentityDiscMatcher() {
  return grey_accessibilityID(kNTPFeedHeaderIdentityDisc);
}

// Matcher for the account menu.
id<GREYMatcher> AccountMenuMatcher() {
  return grey_accessibilityID(kAccountMenuTableViewId);
}

void TapIdentityDisc() {
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      performAction:grey_tap()];
}

void OpenAccountMenu() {
  TapIdentityDisc();
  // Ensure the Account Menu is displayed.
  [[EarlGrey selectElementWithMatcher:AccountMenuMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

id<GREYMatcher> SigninScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

id<GREYMatcher> ManagedProfileCreationScreenMatcher() {
  return grey_accessibilityID(
      kManagedProfileCreationScreenAccessibilityIdentifier);
}

id<GREYMatcher> HistoryScreenMatcher() {
  return grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier);
}

id<GREYMatcher> DefaultBrowserScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier);
}

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> ContinueButtonWithIdentityMatcher(
    FakeSystemIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                 grey_sufficientlyVisible(), nil);

  return matcher;
}

}  // namespace

@interface SeparateProfilesTestCase : ChromeTestCase
@end

@implementation SeparateProfilesTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);
  config.features_enabled.push_back(kUseAccountListFromIdentityManager);
  config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);

  return config;
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::PromoStylePrimaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the enterprise onboarding screen did disapepar.
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSecondaryAccountButtonId)]
      performAction:grey_tap()];

  // Wait for the profile to finish loading again.
  // TODO(crbug.com/331783685): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Verify that the profile was actually switched back.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched");
}

// Tests switching to a managed account and refuse the enterprise onboard
// screen.
// TODO(crbug.com/399015648): Test is flaky on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_testRefuseToSwitchToManageAccount \
  testRefuseToSwitchToManageAccount
#else
#define MAYBE_testRefuseToSwitchToManageAccount \
  FLAKY_testRefuseToSwitchToManageAccount
#endif
- (void)MAYBE_testRefuseToSwitchToManageAccount {
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
  // Refuse the enterprise onboarding screen.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::PromoStyleSecondaryActionButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the new profile to finish loading.
  // TODO(crbug.com/331783685): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

  [SigninEarlGrey verifySignedInWithFakeIdentity:personalIdentity];

  // Verify that the profile was actually switched back.
  GREYAssert(
      [[ChromeEarlGrey currentProfileName] isEqualToString:personalProfileName],
      @"Profile should have been switched");
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
  config.features_enabled.push_back(kUseAccountListFromIdentityManager);
  config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);
  // Note: Can't use the actual feature definition, because its build target
  // depends on a bunch of stuff that mustn't make it into the EG test target.
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");

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

  // Default broser screen: Decline the promo (irrelevant here).
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
// TODO(crbug.com/394536438): Test is flaky.
- (void)FLAKY_testSignInWithManagedAccount {
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenPrimaryButtonMatcher()]
      performAction:grey_tap()];

  // History sync screen: Decline the promo (irrelevant here).
  [ChromeEarlGrey waitForMatcher:HistoryScreenMatcher()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PromoScreenSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Default broser screen: Decline the promo (irrelevant here).
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
