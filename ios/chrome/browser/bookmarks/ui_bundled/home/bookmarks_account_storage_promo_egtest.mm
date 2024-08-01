// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
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

// Bookmark promo integration tests.
@interface BookmarksAccountStoragePromoTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksAccountStoragePromoTestCase

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
}

#pragma mark - BookmarksAccountStoragePromoTestCase Tests

// Tests that the sign-in is re-shown after the user signs-in and then signs-out
// while the bookmarks screen is still shown.
// See http://crbug.com/1432611.
- (void)testPromoReshowAfterSignInAndSignOut {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  // Sign-in with identity1 with the promo.
  [BookmarkEarlGreyUI openBookmarks];
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
  // Sign-out and verify that the promo is shown without the spinner.
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
// from bookmark to sign-in with a different identity.
// See http://crbug.com/1428495.
- (void)testSignInPromoAfterSignOut {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  // Sign-in with identity1.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey signOutAndClearIdentities];
  // Sign-in with bookmark account storage with identity2.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Result: the sign-in is successful without any issue.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity2.userEmail
                                        consent:signin::ConsentLevel::kSignin];
}

// Tests that signin promo is shown even if local data exists.
- (void)testSignInPromoWhenSyncDataNotRemovedIfBatchUploadEnabled {
  // Simulate data from a previous account being leftover by setting
  // kGoogleServicesLastSyncingGaiaId.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey setStringValue:fakeIdentity1.gaiaID
                     forUserPref:prefs::kGoogleServicesLastSyncingGaiaId];

  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Verify that signin promo is visible.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  // Sign in from the promo.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Result: the sign-in is successful without any issue.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity1.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the batch upload dialog is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kBookmarksHomeBatchUploadRecommendationItemIdentifier)];
}

// Tests to sign-in in incognito mode with the promo.
// See http://crbug.com/1432747.
- (void)testSignInPromoInIncognito {
  // Add identity to sign-in with.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  // Open bookmarks in incognito mode.
  [ChromeEarlGrey openNewIncognitoTab];
  [BookmarkEarlGreyUI openBookmarks];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Result: the sign-in is successful without any issue.
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity1.userEmail
                                        consent:signin::ConsentLevel::kSignin];
}

// Tests that account bookmarks are not shown on sign-out.
- (void)testAccountBookmarksNotShownOnSignout {
  // Sign-in with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Add account bookmarks.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify account section shows for a signed-in account.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"Mobile Bookmarks")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign-out.
  [SigninEarlGrey signOut];

  // Verify that the acocunt model is not shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"Mobile Bookmarks")]
      assertWithMatcher:grey_notVisible()];

  // Verify the sign in promo is shown.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that only account bookmarks are not shown on sign-out.
- (void)testOnlyAccountBookmarksNotShownOnSignout {
  // Sign-in with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Add local and account bookmarks to the model.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];

  [BookmarkEarlGreyUI openBookmarks];

  // Verify both local and account sections show for a signed-in account.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_BOOKMARKS_ACCOUNT_SECTION_TITLE)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Sign-out.
  [SigninEarlGrey signOut];

  // Verify the local model still shows but without any titles.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"Mobile Bookmarks")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that account bookmarks are not shown.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_BOOKMARKS_ACCOUNT_SECTION_TITLE)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
