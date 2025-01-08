// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

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

}  // namespace

@interface SeparateProfilesTestCase : ChromeTestCase
@end

@implementation SeparateProfilesTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);
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
  // TODO(crbug.com/375604649): The enterprise onboarding screen should show up
  // at this point.

  // Wait for the new profile to finish loading.
  // TODO(crbug.com/331783685): Find a better way to wait for this.
  GREYWaitForAppToIdle(@"App failed to idle");

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

@end
