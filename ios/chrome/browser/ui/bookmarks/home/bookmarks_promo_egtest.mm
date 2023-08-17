// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

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
  } else if ([self isRunningTest:@selector
                   (testSigninOnlyPromoWithoutAccount_NoSnackbar)]) {
    config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else if ([self isRunningTest:@selector
                   (testSigninOnlyPromoWithoutAccount_WithSnackbar)]) {
    config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else if ([self isRunningTest:@selector(testSigninOnlyPromoWithAccount)] ||
             [self isRunningTest:@selector(testPromoViewBody)]) {
    config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
  } else if ([self isRunningTest:@selector(testPromoViewBodyLegacy)] ||
             [self isRunningTest:@selector
                   (testSignInPromoWithIdentitiesUsingPrimaryButton)] ||
             [self isRunningTest:@selector
                   (testSignInPromoWithIdentitiesUsingSecondaryButton)] ||
             [self isRunningTest:@selector
                   (testSignInPromoWithNoIdentitiesUsingPrimaryButton)]) {
    // TODO(crbug.com/1455018): Re-enable the flag for non-legacy tests.
    config.features_disabled.push_back(syncer::kEnableBookmarksAccountStorage);
  } else if ([self isRunningTest:@selector
                   (testSyncPromoIfSyncToSigninDisabled)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else if ([self isRunningTest:@selector
                   (testNoSyncPromoIfSyncToSigninEnabled)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  return config;
}

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [ChromeEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  [PolicyAppInterface clearPolicies];
}

#pragma mark - BookmarksPromoTestCase Tests

// Tests the promo view body message for sync with
// kEnableBookmarksAccountStorage flag disabled.
- (void)testPromoViewBodyLegacy {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
  NSString* body =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS_WITH_UNITY);
  [[EarlGrey selectElementWithMatcher:grey_text(body)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the promo view body message for signin with
// kEnableBookmarksAccountStorage flag enabled.
- (void)testPromoViewBody {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
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
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
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
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
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
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests the tapping on the primary button of sign-in promo view with identities
// on device makes the confirmaiton sheet appear, and the promo still appears
// after dismissing the sheet.
- (void)testSignInPromoWithIdentitiesUsingPrimaryButton {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Set up a fake identity.
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
}

// Tests the tapping on the secondary button of sign-in promo view with
// identities on device makes the sign-in sheet appear, and the promo still
// appears after dismissing the sheet.
- (void)testSignInPromoWithIdentitiesUsingSecondaryButton {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that sign-in promo view are visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the secondary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Select the identity to dismiss the identity chooser.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that users with a device-level account see a promo whose primary
// button a) signs in, b) hides the promo, c) shows a snackbar with an 'Undo'
// button that signs-out the user when tapped.
// kEnableBookmarksAccountStorage is enabled.
- (void)testSigninOnlyPromoWithAccount {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
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
// SSO Auth flow on tap. Concluding the auth successfully hides the promo and
// shows a snackbar with an 'Undo' button that signs-out the user when tapped.
// kEnableBookmarksAccountStorage is enabled, kReplaceSyncPromosWithSignInPromos
// is disabled.
- (void)testSigninOnlyPromoWithoutAccount_WithSnackbar {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the primary button to start add account flow.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
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
// kEnableBookmarksAccountStorage and kReplaceSyncPromosWithSignInPromos are
// enabled.
- (void)testSigninOnlyPromoWithoutAccount_NoSnackbar {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the primary button to start add account flow.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
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

// Tests that the turn on sync promo is shown if the user is signed in only and
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSyncPromoIfSyncToSigninDisabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableSync:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoVisibleWithMode:
                        SigninPromoViewModeSyncWithPrimaryAccount];
}

// Tests that no sync promo is shown if the user is signed in only and
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testNoSyncPromoIfSyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableSync:NO];
  // By default, `signinWithFakeIdentity` above enables bookmarks data type, so
  // turn it off to ensure that the sync promo isn't ever shown.
  [SigninEarlGreyAppInterface
      setSelectedType:(syncer::UserSelectableType::kBookmarks)
              enabled:NO];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that there is no issue to sign-in only first with an identity using a
// sync passphrase, and then turn on account storage in bookmarks view, using
// the sign-in promo.
// Related to http://crbug.com/1467116.
- (void)testSigninWithSyncPassphraseAndTurnOnSync {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:@"Hello"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in only.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [BookmarkEarlGreyUI openBookmarks];
  // Turn on sync using the sign-in promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kConfirmationAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Verify the sign-in was done.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

@end
