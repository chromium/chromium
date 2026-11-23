// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/public/bookmarks_ui_constants.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/infobar_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Histogram recording the actions on the non-modal bookmark sign-in promo, and
// its `NonModalSignInPromoAction` buckets.
NSString* const kBookmarkPromoHistogram = @"IOS.SignInPromo.NonModal.Bookmark";
constexpr int kPromoAppearBucket = 1;
constexpr int kPromoTimeoutBucket = 3;

}  // namespace

// Test suite for the Bookmarks Editor.
@interface BookmarksEditorTestCase : ChromeTestCase
@end

@implementation BookmarksEditorTestCase

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tear down called once per test.
- (void)tearDownHelper {
  [super tearDownHelper];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tests that editing a bookmark after opening a URL from an external app
// doesn't crash Chrome. Regression test for crbug.com/475777100.
- (void)testEditBookmarkAfterOpeningExternalURL {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];

  // Simulate opening a URL from an external app.
  const GURL externalURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:externalURL];

  // This checks that the editor can be opened again without causing Chrome to
  // crash.
  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];
}

// Tests that swiping down on the bookmark editor dismisses it cleanly and
// triggers coordinator teardown.
- (void)testSwipeDownToDismissBookmarksEditor {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];

  // Swipe down to dismiss BookmarksEditorViewController.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify BookmarksEditorViewController is dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];

  // Verify that editing another page works cleanly without coordinator issues.
  const GURL secondURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:secondURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:secondURL];
  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];
}

// Tests that swiping down when BookmarksFolderChooser is pushed on top of
// BookmarksEditor dismisses the entire stack cleanly without double-pop or
// crash.
- (void)testSwipeDownToDismissBookmarksEditorWithFolderChooserPushed {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];

  // Push BookmarksFolderChooser onto the navigation stack.
  [BookmarkEarlGreyUI openFolderPicker];

  // Verify Folder Chooser is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe down on the modal navigation sheet.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify both view controllers are dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];
}

// Tests swiping down a full 3-level navigation stack:
// [BookmarksEditorVC -> BookmarksFolderChooserVC -> BookmarksFolderEditorVC]
- (void)testSwipeDownToDismissNestedFolderEditorFromBookmarksEditor {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  [BookmarkEarlGreyUI starAndEditCurrentTabWithSnackbarTitle:nil];

  // Push Folder Chooser.
  [BookmarkEarlGreyUI openFolderPicker];

  // Verify Folder Chooser is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Create New Folder" cell in Folder Chooser to push Folder Editor.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier)]
      performAction:grey_tap()];

  // Verify Folder Editor is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Swipe down on the top Folder Editor VC.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Verify the entire navigation stack is dismissed.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];
}

// Tests that signing out while editing an account bookmark dismisses the
// editor gracefully.
- (void)testSignOutWhileEditingAccountBookmark {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGrey addBookmarkWithTitle:@"Account Bookmark"
                                     URL:@"https://www.example.com"
                               inStorage:BookmarkStorageType::kAccount];

  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI
      openMobileBookmarks:chrome_test_util::KindOfTest::kAccount];

  // Long press on bookmark and tap Edit.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TappableBookmarkNodeWithLabel(
                                   @"Account Bookmark",
                                   chrome_test_util::KindOfTest::kAccount)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Verify editor is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Sign out while editor is open.
  [SigninEarlGrey signOut];

  // Editor should be dismissed gracefully.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];
}

// Tests that the bookmarks editor opened while the non-modal sign-in promo
// banner is visible is not dismissed when the banner times out. Previously the
// banner was presented directly on the BVC, so presenting the editor over it
// caused the banner's timeout dismissal to dismiss the editor too.
- (void)testEditorStaysOpenAfterPromoBannerTimeout {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.iph_feature_enabled = "IPH_iOSPromoNonModalSigninBookmark";
  // Let the promo banner time out while the editor is presented.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kUseDefaultInfobarDuration);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [BookmarkEarlGreyUI starCurrentTab];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kInfobarBannerViewIdentifier)];

  // Open the editor from the snackbar while the banner is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSnackbarButtonAccessibilityId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kBookmarkEditViewContainerIdentifier)];

  // Wait for the banner to time out.
  ConditionBlock timedOut = ^{
    return [MetricsAppInterface expectCount:1
                                  forBucket:kPromoTimeoutBucket
                               forHistogram:kBookmarkPromoHistogram] == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kInfobarBannerDefaultPresentationDuration +
                     base::test::ios::kWaitForUIElementTimeout,
                 timedOut),
             @"The promo banner didn't time out.");

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
       expectCount:1
         forBucket:kPromoAppearBucket
      forHistogram:kBookmarkPromoHistogram]);
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
}

@end
