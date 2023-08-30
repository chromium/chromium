// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/base/consent_level.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::DeleteButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::ReadingListMarkAsReadButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;
using reading_list_test_utils::AddedToLocalReadingListSnackbar;
using reading_list_test_utils::AddURLToReadingList;
using reading_list_test_utils::OpenReadingList;
using reading_list_test_utils::ReadingListItem;
using reading_list_test_utils::VisibleReadingListItem;

namespace {

NSString* const kReadTitle = @"foobar";
NSString* const kReadURL = @"http://readfoobar.com";
NSString* kPage1Title = @"Page 1 Title";
const char kPage1URL[] = "/page1";
NSString* kPage2Title = @"Page 2 Title";
const char kPage2URL[] = "/page2";
constexpr base::TimeDelta kLongPressDuration = base::Seconds(1);
constexpr base::TimeDelta kSyncInitializedTimeout = base::Seconds(5);

id<GREYMatcher> SignedInSnackbar(NSString* email) {
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS, base::SysNSStringToUTF16(email));
  return grey_text(snackbarMessage);
}

id<GREYMatcher> SignedInSnackbarUndoButton() {
  return grey_accessibilityID(kSigninSnackbarUndo);
}

id<GREYMatcher> AddedToAccountReadingListSnackbar(NSString* email) {
  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_FOR_ACCOUNT);
  std::u16string utf16Text = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "count", 1, "email", base::SysNSStringToUTF16(email));
  NSString* snackbarMessage = base::SysUTF16ToNSString(utf16Text);
  return grey_allOf(
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier"),
      grey_text(snackbarMessage), nil);
}

id<GREYMatcher> AddedToAccountReadingListSnackbarUndoButton() {
  return grey_accessibilityID(kReadingListAddedToAccountSnackbarUndoID);
}

// The cloud slash icon that appears for Reading List items that are only stored
// in the local storage. Shown only for signed-in users.
id<GREYMatcher> VisibleLocalItemIcon(NSString* title) {
  return grey_allOf(grey_ancestor(ReadingListItem(title)),
                    grey_accessibilityID(kTableViewURLCellMetadataImageID),
                    grey_sufficientlyVisible(), nil);
}

// Provides responses containing a custom title for fake URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    response->set_content("<html><head><title>" +
                          base::SysNSStringToUTF8(kPage1Title) +
                          "</title></head></html>");
    return std::move(response);
  }
  if (request.relative_url == kPage2URL) {
    response->set_content("<html><head><title>" +
                          base::SysNSStringToUTF8(kPage2Title) +
                          "</title></head></html>");
    return std::move(response);
  }

  return nil;
}

}  // namespace

// Reading List integration tests for Chrome with account storage and UI
// enabled.
@interface ReadingListAccountStorageTestCase : ChromeTestCase
@end

@implementation ReadingListAccountStorageTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDown {
  GREYAssertNil([ReadingListAppInterface clearEntries],
                @"Unable to clear Reading List entries");
  [ChromeEarlGrey clearBrowsingHistory];
  // Prevent failure due to clear browsing data spinner. Should be called
  // before [super tearDown] which calls sign-out.
  // TODO(crbug.com/1451733): Remove this when ChromeTestCase will always wait
  // for sign-out completion.
  [ChromeEarlGrey signOutAndClearIdentitiesAndWaitForCompletion];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncInitializedTimeout];
  // Shutdown network process after tests run to avoid hanging from
  // clearing browsing history.
  [ChromeEarlGrey killWebKitNetworkProcess];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      syncer::kReadingListEnableDualReadingListModel);
  config.features_enabled.push_back(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
  if ([self isRunningTest:@selector
            (testSignInWithSecondaryAccountInPromo_WithSnackbar)] ||
      [self isRunningTest:@selector(testAddAccountItemThenUpgradeToFullSync)] ||
      [self isRunningTest:@selector(testAddItemWithFullSync)] ||
      [self isRunningTest:@selector(testUndoAddItemWithFullSync)] ||
      [self isRunningTest:@selector(testPromoNotShownWhenSyncDataNotRemoved)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector
            (testSignInWithSecondaryAccountInPromo_NoSnackbar)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
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
// with a secondary identity using the secondary button in the promo, and a
// snackbar is shown after sign-in.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSignInWithSecondaryAccountInPromo_WithSnackbar {
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

// Test that when multiple identities exist on the device, the user can sign-in
// with a secondary identity using the secondary button in the promo.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testSignInWithSecondaryAccountInPromo_NoSnackbar {
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

// Tests to sign-in with one identity, sign-out, and use the sign-in promo
// from Reading List to sign-in with a different identity.
- (void)testPromoSignInAfterSignOut {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  // Sign-in with identity1 with the promo.
  OpenReadingList();
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SignedInSnackbar(
                                              fakeIdentity1.userEmail)];
  // Sign-out & sign-in with the identity2.
  [SigninEarlGrey signOut];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // The sign-in snackbar is only displayed if the feature
  // syncer::kReplaceSyncPromosWithSignInPromos is disabled, as per logic
  // introduced in https://crrev.com/c/4733378.
  if (![ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:SignedInSnackbar(
                                                fakeIdentity2.userEmail)];
  }

  // Verify that the second account is signed-in.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity2.userEmail
                                        consent:signin::ConsentLevel::kSignin];
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
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Verify that the right snackbar appears and there's no undo button on it.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:AddedToLocalReadingListSnackbar()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(AddedToAccountReadingListSnackbarUndoButton(),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  // Verify there's no cloud icon on the new item in the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_nil()];
}

// Add a page when signed-out and another after sign-in with full sync. Test
// that both items do not have the cloud icon in the Reading List.
- (void)testAddItemWithFullSync {
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Sign-in with full sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncInitializedTimeout];
  // Add Page 2 to the Reading List and verify that the snackbar containing the
  // user's email and an undo button appears.
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:AddedToAccountReadingListSnackbar(
                                              fakeIdentity.userEmail)];
  // Verify that the new items are shown, and there's no cloud icon on the them
  // in the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage2Title)]
      assertWithMatcher:grey_nil()];
}

// When signed-in with full sync, test that tapping on "Undo" from the "item
// added" snackbar removes the item from the Reading List.
- (void)testUndoAddItemWithFullSync {
  // Sign-in with full sync.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncInitializedTimeout];
  // Add Page 1 to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Tap on undo when the snackbar appears.
  [ChromeEarlGrey
      waitForAndTapButton:grey_allOf(
                              AddedToAccountReadingListSnackbarUndoButton(),
                              grey_sufficientlyVisible(), nil)];
  // Verify that Page 1 is not in the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_nil()];
}

// Add a page when signed-out and another after sign-in with the Reading List
// promo. Test that only the first item has the cloud icon in the Reading List.
- (void)testAddItemWithAccountStorage {
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Sign-in with the Reading List promo.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Ensure that the first sync spinner has disappeared.
  [ChromeEarlGreyUI waitForAppToIdle];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Verify that the cloud icon is shown on the first item.
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add Page 2 to the Reading List and verify that the snackbar containing the
  // user's email and an undo button appears.
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:AddedToAccountReadingListSnackbar(
                                              fakeIdentity.userEmail)];
  // Verify that both items are visible in the Reading List, and that there's
  // one cloud icon on the first item, but none on the second.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage2Title)]
      assertWithMatcher:grey_nil()];
}

// When signed-in with the Reading List promo, test that tapping on "Undo" from
// the "item added" snackbar removes the item from the Reading List.
- (void)testUndoAddItemWithAccountStorage {
  // Sign-in with the Reading List promo.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add Page 1 to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Tap on undo when the snackbar appears.
  [ChromeEarlGrey
      waitForAndTapButton:grey_allOf(
                              AddedToAccountReadingListSnackbarUndoButton(),
                              grey_sufficientlyVisible(), nil)];
  // Verify that Page 1 is not in the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_nil()];
}

// Test that the item added to account Reading List disappears when signed-out.
- (void)testAddAccountItemThenSignOut {
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Sign-in with fakeIdentity in the Reading List.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add Page 2 to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  // Sign-out.
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncInitializedTimeout];
  // Verify that only Page 1 is visible with no cloud icon.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage2Title)]
      assertWithMatcher:grey_nil()];
}

// Test that if an item is added before sign-in and another is added after
// account storage sign-in, then the user upgrades to full sync, both items are
// visible and do not have the cloud icon.
- (void)testAddAccountItemThenUpgradeToFullSync {
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  // Sign-in with fakeIdentity in the Reading List.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Dismiss the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add Page 2 to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  // Upgrade to full sync.
  [ChromeEarlGreyUI openSettingsMenu];
  id<GREYMatcher> syncCell =
      grey_allOf(grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:syncCell] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  id<GREYMatcher> confirmSyncButton =
      grey_allOf(grey_accessibilityID(kConfirmationAccessibilityIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:confirmSyncButton]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncInitializedTimeout];
  // Dismiss Settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Verify that both items are shown without cloud icon.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleLocalItemIcon(kPage2Title)]
      assertWithMatcher:grey_nil()];
  // Dismiss the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Sign-out and clear data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncInitializedTimeout];
  // Verify that no item is in the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_nil()];
}

// Test that after sign-in with the Reading List promo, if two items are added
// and one is removed, then after a sign-out and a new sign-in with the Reading
// List sign-in promo with the same account, the removed item is not visible.
- (void)testRemoveItemAfterSignInThenRefreshSignin {
  // Sign-in with the Reading List Promo.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add pages to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  // Remove Page 1 from the Reading List.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      performAction:grey_longPressWithDuration(kLongPressDuration)];
  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];
  // Verify that only Page 2 is in the Reading List.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:VisibleReadingListItem(kPage2Title)];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_nil()];
  // Sign-out and sign-in with the same account.
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncInitializedTimeout];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Verify that only the page 2 is still in the Reading list.
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
}

// Test that after a sign-in with the Reading List sign-in promo, if two unread
// entries are added, then the first item is marked as unread, the read and
// unread items sections should be shown correctly and remain so after a
// sign-out & sign-in with the same account.
- (void)testMoveItemThenRefreshSignIn {
  // Sign-in with the Reading List Promo.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  OpenReadingList();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Close the Reading List.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewNavigationDismissButtonId)]
      performAction:grey_tap()];
  // Add pages to the Reading List.
  AddURLToReadingList(self.testServer->GetURL(kPage1URL));
  AddURLToReadingList(self.testServer->GetURL(kPage2URL));
  // Mark Page 1 as read.
  OpenReadingList();
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      performAction:grey_longPressWithDuration(kLongPressDuration)];
  [[EarlGrey selectElementWithMatcher:ReadingListMarkAsReadButton()]
      performAction:grey_tap()];
  // Wait one second since the reading list items may update multiple times.
  // TODO(crbug.com/1445875): Check if this delay can be replaced by the use of
  // waitForUIElementToAppearWithMatcher instead.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  // Verify that the unread and the read sections headers are visible.
  NSString* readHeaderText =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_READ_HEADER);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(readHeaderText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  NSString* unreadHeaderText =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_UNREAD_HEADER);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(unreadHeaderText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  // Verify that both items are visible and only one of them is unread.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:ReadingListItem(kPage1Title)];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual([ReadingListAppInterface readEntriesCount], 1,
                  @"The read entries count is incorrect.");
  GREYAssertEqual([ReadingListAppInterface unreadEntriesCount], 1,
                  @"The unread entries count is incorrect.");
  // Sign-out and sign-in with the same account.
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncInitializedTimeout];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncInitializedTimeout];
  // Verify that both items are visible and only one of them is unread.
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage1Title)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:VisibleReadingListItem(kPage2Title)]
      assertWithMatcher:grey_notNil()];
  GREYAssertEqual([ReadingListAppInterface readEntriesCount], 1,
                  @"The read entries count is incorrect.");
  GREYAssertEqual([ReadingListAppInterface unreadEntriesCount], 1,
                  @"The unread entries count is incorrect.");
}

@end
