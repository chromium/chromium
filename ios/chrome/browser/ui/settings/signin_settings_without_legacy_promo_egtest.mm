// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::SettingsSignInRowMatcher;

@interface SigninSettingsWithoutLegacyPromoTestCase : ChromeTestCase
@end

@implementation SigninSettingsWithoutLegacyPromoTestCase

- (void)tearDown {
  [PolicyAppInterface clearPolicies];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kHideSettingsSyncPromo);
  return config;
}

- (void)testPromoCardHidden {
  [ChromeEarlGreyUI openSettingsMenu];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// For a signed out user without device accounts, tests that the "Turn on Sync"
// row is shown and opens the appropriate dialog upon tap.
- (void)testEnableSyncRowOpensDialogIfSignedOutAndNoDeviceAccounts {
  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with device accounts, tests that the "Turn on Sync" row
// is shown and opens the appropriate dialog upon tap.
- (void)testEnableSyncRowOpensDialogIfSignedOutAndSomeDeviceAccounts {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with the SyncDisabled policy, tests that the "Turn on
// Sync" row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testEnableSyncRowDisabledBySyncPolicy {
  [ChromeEarlGreyUI openSettingsMenu];
  // Set policy after opening settings so there's no need to dismiss the policy
  // bottom sheet.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with the BrowserSignin policy, tests that the "Turn on
// Sync" row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testEnableSyncRowDisabledBySigninPolicy {
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);
  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed in user, tests that the "Sync off" row is shown and opens the
// dialog to enable sync upon tap.
- (void)testSyncOffRowOpensDialogIfSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed in user with the SyncDisabled policy, tests that the "Sync off"
// row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testSyncOffRowDisabledByPolicy {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Set policy after opening settings so there's no need to dismiss the policy
  // bottom sheet.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SYNC_SETTINGS_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
