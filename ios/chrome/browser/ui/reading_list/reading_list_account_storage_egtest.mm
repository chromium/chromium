// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/base/consent_level.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using reading_list_test_utils::AddedToLocalReadingListSnackbar;
using reading_list_test_utils::AddURLToReadingList;
using reading_list_test_utils::OpenReadingList;
using reading_list_test_utils::ReadingListItem;

namespace {

NSString* const kReadTitle = @"foobar";
NSString* const kReadURL = @"http://readfoobar.com";
NSString* kNewItemTitle = @"New Item";
const char kNewItemURL[] = "/newItem";

id<GREYMatcher> SignedInSnackbar(NSString* email) {
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS, base::SysNSStringToUTF16(email));
  return grey_text(snackbarMessage);
}

id<GREYMatcher> SignedInSnackbarUndoButton() {
  return grey_accessibilityID(kSigninSnackbarUndo);
}

id<GREYMatcher> AddedToAccountStorageSnackbarUndoButton() {
  return grey_accessibilityID(kReadingListAddedToAccountSnackbarUndoID);
}

// The cloud slash icon that appears for Reading List items that are only stored
// in the local storage. Shown only for signed-in users.
id<GREYMatcher> LocalItemIcon(NSString* title) {
  return grey_allOf(grey_ancestor(ReadingListItem(title)),
                    grey_accessibilityID(kTableViewURLCellMetadataImageID),
                    nil);
}

// Provides responses containing a custom title for fake URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);

  if (request.relative_url == kNewItemURL) {
    response->set_content("<html><head><title>" +
                          base::SysNSStringToUTF8(kNewItemTitle) +
                          "</title></head></html>");
    return std::move(response);
  }

  return nil;
}

}  // namespace

// Reading List integration tests for Chrome with account storage and UI
// enabled.
@interface ReadingListAccountStorageTestCase : WebHttpServerChromeTestCase
@end

@implementation ReadingListAccountStorageTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBrowsingHistory];
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      syncer::kReadingListEnableDualReadingListModel);
  config.features_enabled.push_back(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
  return config;
}

#pragma mark - Sign-in promo and snackbar

// Test that the Reading List sign-in promo is in the "no accounts" mode when
// there is no identity on the device.
- (void)testPromoWithNoMainAccount {
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Test that the Reading List sign-in promo show the existing identity when an
// identity exists on the device.
- (void)testPromoWithMainAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Test that when an identity is available on the device, the user can sign-in
// with a tap on the promo primary button, and when the sign-in is done, a
// snackbar with the user's email and a undo button is shown.
- (void)testSignInWithPromoPrimaryButton {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in with the existing identity with the promo.
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Wait for the signed-in snackbar with undo button.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbar(
                                              fakeIdentity.userEmail)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbarUndoButton()];
  // Verify that the identity is signed-in without sync and the promo is hidden.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Test that when the "undo" button on the signed-in snackbar is tapped after a
// successful sign-in using the promo, the snackbar disappears, the identity is
// signed-out, and the promo reappears.
- (void)testUndoSignInWithSnackbar {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in with the existing identity with the promo.
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify the snackbar is shown after sign-in and the identity is signed-in.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbar(
                                              fakeIdentity.userEmail)];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Tap on "Undo".
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SignedInSnackbarUndoButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the snackbar disappears, the promo is shown, and the user is
  // signed-out.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SignedInSnackbar(
                                                 fakeIdentity.userEmail)];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [SigninEarlGrey verifySignedOut];
}

// Test that when multiple identities exist on the device, the user can sign-in
// with a secondary identity using the secondary button in the promo.
- (void)testSignInWithSecondaryAccountInPromo {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  // Use sign-in with the second account using the promo's secondary button.
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  // Verify that the signed-in snackbar appears with the correct email.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbar(
                                              fakeIdentity2.userEmail)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbarUndoButton()];
  // Verify that the identity2 is signed-in without sync, and that the promo is
  // hidden.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity2.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Test that if the identity is signed-in with full sync (sync feature) enabled,
// the Reading List promo is hidden.
- (void)testPromoHiddenAfterSignInWithFullSync {
  // Sign-in with full sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  // Verify that the promo is hidden in the Reading List.
  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Test that if the identity is signed-in with sync and account storage both
// disabled, the Reading List promo is hidden.
- (void)testPromoHiddenAfterSignInWithoutAccountStorageOrSync {
  // Sign-in without full sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  // Verify that the promo is hidden in the Reading List.
  OpenReadingList();
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in is re-shown after the user signs-in and then signs-out
// while the reading list screen is still shown.
// See http://crbug.com/1432611.
- (void)testPromoReshowAfterSignInAndSignOut {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  // Sign-in with identity1 with the promo.
  OpenReadingList();
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

  OpenReadingList();
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

  OpenReadingList();
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
  OpenReadingList();
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
  OpenReadingList();
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

#pragma mark - Local & account storage items

// When adding an item and there's no signed-in account, test that the standard
// "Added to Reading List" snackbar is shown and there's no cloud icon on the
// new item.
- (void)testAddItemWhenSignedOut {
  AddURLToReadingList(self.testServer->GetURL(kNewItemURL));
  // Verify that the right snackbar appears and there's no undo button on it.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:AddedToLocalReadingListSnackbar()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   AddedToAccountStorageSnackbarUndoButton(),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  // Verify there's no cloud icon on the new item in the Reading List.
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(LocalItemIcon(kNewItemTitle),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
