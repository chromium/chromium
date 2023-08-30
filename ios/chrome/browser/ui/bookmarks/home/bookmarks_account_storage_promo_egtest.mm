// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/signin/public/base/consent_level.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

// Bookmark promo integration tests for Chrome with
// kEnableBookmarksAccountStorage enabled.
@interface BookmarksAccountStoragePromoTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksAccountStoragePromoTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(syncer::kEnableBookmarksAccountStorage);
  if ([self isRunningTest:@selector
            (testPromoViewNotShownWhenSyncDataNotRemoved)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
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
  // Sign-in+sync with identity1.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableSync:YES];
  [ChromeEarlGrey signOutAndClearIdentitiesAndWaitForCompletion];
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

// Tests that the signin promo is not shown when last signed-in user did not
// remove data during sign-out.
- (void)testPromoViewNotShownWhenSyncDataNotRemoved {
  // Sign-in with sync with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];
  // Sign-out without removing data.
  [SigninEarlGrey signOut];

  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
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

@end
