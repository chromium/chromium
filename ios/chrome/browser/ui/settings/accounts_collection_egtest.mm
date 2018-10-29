// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_control_item.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountsSyncButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

typedef NSString* (^ExpectedTextLabelCallback)(NSString* identityEmail);

namespace {

// Returns a matcher for a button that matches the userEmail in the given
// |identity|.
id<GREYMatcher> ButtonWithIdentity(ChromeIdentity* identity) {
  return ButtonWithAccessibilityLabel(identity.userEmail);
}
}

// Integration tests using the Account Settings screen.
@interface AccountCollectionsTestCase : ChromeTestCase
@end

@implementation AccountCollectionsTestCase

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |identity|, then open the Sync Settings.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:AccountsSyncButton()];

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils assertSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils assertSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity2);

  // Sign In |identity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |identity2| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity2)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that |identity2| isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity2.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGreyUtils assertSignedInWithIdentity:identity1];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Sync Settings screen is correctly reloaded when one of the
// secondary accounts disappears.
- (void)testSignInReloadSyncOnForgetIdentity {
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity2);

  // Sign In |identity|, then open the Sync Settings.
  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:AccountsSyncButton()];

  // Forget |identity2|, allowing the UI to synchronize before and after
  // forgetting the identity.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  identity_service->ForgetIdentity(identity2, nil);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that both |identity1| and |identity2| aren't shown in the Sync
  // Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity1.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity2.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGreyUtils assertSignedInWithIdentity:identity1];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |identity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |identity| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils assertSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the user isn't signed out and the UI is correct when the
// disconnect is cancelled in the Account Settings screen.
- (void)testSignInDisconnectCancelled {
// TODO(crbug.com/669613): Re-enable this test on devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |identity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the "Disconnect Account" dialog, then tap "Cancel".
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];

  // Check that Account Settings screen is open and |identity| is signed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsAccountsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils assertSignedInWithIdentity:identity];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks if the sync cell is correctly configured with the expected detail text
// label and an image.
- (void)checkSyncCellWithExpectedTextLabelCallback:
    (ExpectedTextLabelCallback)callback {
  NSAssert(callback, @"Need callback");
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |identity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  NSString* expectedDetailTextLabel = callback([identity userEmail]);
  // Check that account sync button displays the expected detail text label and
  // an image.
  GREYPerformBlock block = ^BOOL(id element, NSError* __strong* errorOrNil) {
    GREYAssertTrue([element isKindOfClass:[AccountControlCell class]],
                   @"Should be AccountControlCell type");
    AccountControlCell* cell = static_cast<AccountControlCell*>(element);
    return
        [cell.detailTextLabel.text isEqualToString:expectedDetailTextLabel] &&
        cell.imageView.image != nil;
  };
  [[EarlGrey selectElementWithMatcher:AccountsSyncButton()]
      performAction:[GREYActionBlock
                        actionWithName:@"Invoke clearStateForTest selector"
                          performBlock:block]];
}

// Tests the sync cell is correctly configured when having a MDM error.
- (void)testMDMError {
  ios::FakeChromeIdentityService* fakeChromeIdentityService =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  fakeChromeIdentityService->SetFakeMDMError(true);
  ExpectedTextLabelCallback callback = ^(NSString* identityEmail) {
    return l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SYNC_ERROR);
  };
  [self checkSyncCellWithExpectedTextLabelCallback:callback];
}

// Tests the sync cell is correctly configured when no error.
- (void)testSyncItemWithSyncingMessage {
  ExpectedTextLabelCallback callback = ^(NSString* identityEmail) {
    return l10n_util::GetNSStringF(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNCING,
                                   base::SysNSStringToUTF16(identityEmail));
  };
  [self checkSyncCellWithExpectedTextLabelCallback:callback];
}

// Tests the sync cell is correctly configured when the passphrase is required.
- (void)testSyncItemWithPassphraseRequired {
  ExpectedTextLabelCallback callback = ^(NSString* identityEmail) {
    ios::ChromeBrowserState* browser_state =
        chrome_test_util::GetOriginalBrowserState();
    browser_sync::ProfileSyncService* profile_sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(browser_state);
    profile_sync_service->GetEncryptionObserverForTest()->OnPassphraseRequired(
        syncer::REASON_DECRYPTION,
        syncer::KeyDerivationParams::CreateForPbkdf2(),
        sync_pb::EncryptedData());
    return l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION);
  };
  [self checkSyncCellWithExpectedTextLabelCallback:callback];
}

@end
