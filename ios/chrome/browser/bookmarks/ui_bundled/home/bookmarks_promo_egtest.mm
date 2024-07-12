// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::FakeAddAccountScreenCancelButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::IdentityChooserScrim;
using chrome_test_util::ManageSyncSettingsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;

// Bookmark promo integration tests for Chrome.
@interface BookmarksPromoTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksPromoTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testSyncTypesListDisabled)]) {
    // Configure the policy.
    config.additional_args.push_back(
        "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
    config.additional_args.push_back(
        "<dict><key>SyncTypesListDisabled</key><array><string>bookmarks</"
        "string></array></dict>");
  }

  return config;
}

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  [PolicyAppInterface clearPolicies];
}

#pragma mark - BookmarksPromoTestCase Tests

// Tests the promo view body message for signin.
- (void)testPromoViewBody {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  NSString* body = l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS);
  NSString* primaryButtonText =
      l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN);
  [[EarlGrey selectElementWithMatcher:grey_text(body)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(primaryButtonText)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarkEarlGrey setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Go to child node.
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Wait until promo is gone.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the promo already seen state is not updated.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];

  // Come back to root node, and the promo view should appear.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Check promo view is still visible.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping No thanks on the promo make it disappear.
- (void)testPromoNoThanksMakeItDisappear {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarkEarlGrey setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Tap the dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Wait until promo is gone.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the promo already seen state is updated.
  [BookmarkEarlGrey verifyPromoAlreadySeen:YES];
}

// Tests the tapping on the primary button of sign-in promo view with no
// identities on device makes the sign-in sheet appear, and the promo still
// appears after dismissing the sheet.
- (void)testSignInPromoWithNoIdentitiesUsingPrimaryButton {
  [BookmarkEarlGreyUI openBookmarks];

  // Check that sign-in promo view are visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Tap the primary button.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:FakeAddAccountScreenCancelButton()]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests the tapping on the secondary button of sign-in promo view with
// identities on device makes the sign-in sheet appear, and the promo still
// appears after dismissing the sheet.
- (void)testSignInPromoWithIdentitiesUsingSecondaryButton {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that sign-in promo view are visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the secondary button.
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];

  // Tap the scrim to dismiss the the chooser.
  [[EarlGrey selectElementWithMatcher:IdentityChooserScrim()]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that users with a device-level account see a promo whose primary
// button a) signs in, b) hides the promo, c) shows a snackbar with an 'Undo'
// button that signs-out the user when tapped.
- (void)testSigninOnlyPromoWithAccount {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the primary button.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];

  // Verify the snackbar is shown after sign-in and tap 'Undo'.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                              base::SysNSStringToUTF16(fakeIdentity.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninSnackbarUndo),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

// Tests that users with no device-level account see a promo that leads to an
// SSO Auth flow on tap. Concluding the auth successfully hides the promo.
- (void)testSigninOnlyPromoWithoutAccount {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Tap the primary button to start add account flow.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  // Set up a fake identity to add and sign-in with.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Make sure the fake SSO view controller is fully removed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the user got signed in and the promo hidden.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in promo should not be shown after been shown 19 times.
- (void)testAutomaticSigninPromoDismiss {
  [BookmarkEarlGrey setPromoAlreadySeenNumberOfTimes:19];
  [BookmarkEarlGreyUI openBookmarks];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  // Check the sign-in promo already-seen state didn't change.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  GREYAssertEqual(20, [BookmarkEarlGrey numberOfTimesPromoAlreadySeen],
                  @"Should have incremented the display count");

  // Close the bookmark view and open it again.
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];
  [BookmarkEarlGreyUI openBookmarks];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in promo isn't shown when the SyncDisabled policy is
// enabled.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  // Dismiss the popup.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Check that the sign-in promo is not visible anymore.
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in promo isn't shown when the SyncTypesListDisabled
// bookmarks item policy is selected.
- (void)testSyncTypesListDisabled {
  // Check that the sign-in promo is not visible anymore.
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that account settings promo is displayed when the bookmark view is
// opened from an incognito tab.
// See: crbug.com/339472472.
- (void)testAccountSettingsHiddenFromIncognitoTab {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  [ChromeEarlGrey openNewIncognitoTab];
  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that review account settings promo is shown if the user is signed in
// only but bookmarks account storage is off and gets removed after enabling
// bookmarks.
- (void)testAccountSettingsPromoWithBookmarksOff {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Turn Bookmarks On.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncBookmarksIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/YES)];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify that the promo disappears.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SettingsDoneButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that review account settings promo is not shown if the user is signed
// in only and bookmarks account storage is already enabled.
- (void)testAccountSettingsPromoWithBookmarksOn {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that account settings are viewed from the bookmarks manager and account
// gets removed.
- (void)testAccountSettingsViewedFromBookmarksManager {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Remove identity from device.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Verify that Account Settings is closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
  // Sign in promo shows.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests review account settings promo changes to a sign-in promo after signing
// out from account settings.
- (void)testSignOutFromAccountSettingsFromBookmarksManager {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSignedInWithPrimaryAccount];

  // Open the settings using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Scroll to the bottom to view the signout button.
  id<GREYMatcher> scroll_view_matcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scroll_view_matcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Verify that Account Settings is closed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Dismiss sign out snackbar.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE))]
      performAction:grey_tap()];

  // Sign in promo shows and try to sign in succeeds.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     (IDS_IOS_SIGNIN_PROMO_REVIEW_BOOKMARKS_SETTINGS)))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Tests the review account settings promo does not show after signing in as
// bookmarks gets enabled by default on sign-in.
- (void)testNoReviewAccountSettingsPromo {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Bookmarks promo.
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSStringF(
                                   IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                                   base::SysNSStringToUTF16(
                                       fakeIdentity1.userEmail)))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify Account Settings promo does not show.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Verify that the bookmarks type is now enabled.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kBookmarks],
      @"Bookmarks should be enabled.");
}

// Tests that bookmarks type gets disabled as it was before signing in when the
// snackbar undo is tapped.
- (void)testUndoSignInTypeDisabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off.
  [SigninEarlGrey setSelectedType:(syncer::UserSelectableType::kBookmarks)
                          enabled:NO];

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Bookmarks promo.
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap undo button from the snackbar.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
      base::SysNSStringToUTF16(fakeIdentity1.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninSnackbarUndo),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Sign back in without using the promo.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Verify that the bookmarks type is disabled as it was before signing in.
  GREYAssertFalse(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kBookmarks],
      @"Bookmarks should be disabled.");
}

// Tests that bookmarks type remains enabled as it was before signing in even
// when the snackbar undo is tapped.
- (void)testUndoSignInTypeEnabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Make sure bookamrks type is enabled.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kBookmarks],
      @"Bookmarks should be enabled.");

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Sign in from Bookmarks promo.
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap undo button from the snackbar.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
      base::SysNSStringToUTF16(fakeIdentity1.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_text(snackbarMessage)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninSnackbarUndo),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Sign back in without using the promo.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];

  // Verify that the bookmarks type remains enabled as it was before signing in.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kBookmarks],
      @"Bookmarks should be enabled.");
}

@end
