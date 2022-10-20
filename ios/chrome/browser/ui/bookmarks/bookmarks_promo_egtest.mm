// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarkHomeDoneButton;
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
  }
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
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

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
  [BookmarkEarlGrey setupStandardBookmarks];
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
  [BookmarkEarlGrey setupStandardBookmarks];
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
  [BookmarkEarlGrey setupStandardBookmarks];
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
  [BookmarkEarlGrey setupStandardBookmarks];
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
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
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

@end
