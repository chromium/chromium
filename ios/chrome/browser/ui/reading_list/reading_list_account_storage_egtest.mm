// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::PrimarySignInButton;

namespace {
NSString* const kReadTitle = @"foobar";
NSString* const kReadURL = @"http://readfoobar.com";
}  // namespace

// Reading List integration tests for Chrome with account storage and UI
// enabled.
@interface ReadingListAccountStorageTestCase : WebHttpServerChromeTestCase
@end

@implementation ReadingListAccountStorageTestCase

- (void)tearDown {
  [super tearDown];
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      reading_list::switches::kReadingListEnableDualReadingListModel);
  config.features_enabled.push_back(
      reading_list::switches::kReadingListEnableSyncTransportModeUponSignIn);
  return config;
}

#pragma mark - ReadingListAccountStorageTestCase Tests

// Tests that the sign-in is re-shown after the user signs-in and then signs-out
// while the reading list screen is still shown.
// See http://crbug.com/1432611.
- (void)testPromoReshowAfterSignInAndSignOut {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  // Sign-in with identity1 with the promo.
  [ReadingListEarlGreyUI openReadingList];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that identity1 is signed-in and the promo is hidden.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity1.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  // Sign-out without changing the UI and verify that the promo is shown,
  // without spinner.
  [SigninEarlGrey signOut];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoActivityIndicatorId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that the signin promo is shown again when last signed-in user removes
// data during sign-out.
- (void)testPromoShownWhenSyncDataIsRemoved {
  // Sign-in with sync with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];
  // Sign-out and remove data.
  [ChromeEarlGrey signOutAndClearIdentitiesAndWaitForCompletion];

  [ReadingListEarlGreyUI openReadingList];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests that the signin promo is not shown when last signed-in user did not
// remove data during sign-out.
- (void)testPromoNotShownWhenSyncDataNotRemoved {
  // Sign-in with sync with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];
  // Sign-out without removing data.
  [SigninEarlGrey signOut];

  [ReadingListEarlGreyUI openReadingList];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests to sign-in in incognito mode with the promo.
// See http://crbug.com/1432747.
- (void)testSignInPromoInIncognito {
  // Add identity to sign-in with.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  // Open the Reading List in incognito mode.
  [ChromeEarlGrey openNewIncognitoTab];
  [ReadingListEarlGreyUI openReadingList];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::PrimarySignInButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Result: the sign-in is successful without any issue.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity1.userEmail
                                        consent:signin::ConsentLevel::kSignin];
}

// Tests that if the data is reloaded after the account storage promo is shown,
// the promo item is still shown.
// See https://crbug.com/1439243.
- (void)testPromoShownAfterContentReload {
  [ReadingListEarlGreyUI openReadingList];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  GREYAssertNil(
      [ReadingListAppInterface addEntryWithURL:[NSURL URLWithString:kReadURL]
                                         title:kReadTitle
                                          read:YES],
      @"Unable to add Reading List item");
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

@end
