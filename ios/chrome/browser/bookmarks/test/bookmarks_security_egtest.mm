// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/public/bookmarks_ui_constants.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::TappableBookmarkNodeWithLabel;

@interface BookmarksSecurityTestCase : ChromeTestCase
@end

@implementation BookmarksSecurityTestCase

- (void)setUp {
  [super setUp];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

- (void)tearDownHelper {
  [ChromeCoordinatorAppInterface reset];
  [super tearDownHelper];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tests that a bookmarklet executed during Bookmarks UI dismissal is blocked
// if the active tab navigated to a different origin in the background during
// the animation.
- (void)testBookmarkletTOCTOUMitigation {
  // Add the bookmarklet programmatically.
  NSString* bookmarkletURL =
      @"javascript:document.getElementById('result').innerText='EXECUTED';";
  [BookmarkEarlGrey addBookmarkWithTitle:@"TOCTOU_Bookmarklet"
                                     URL:bookmarkletURL
                               inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Start the test server.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load the attacker page.
  GURL attackerURL = self.testServer->GetURL("/toctou_attacker.html");
  [ChromeEarlGrey loadURL:attackerURL];

  // Open Bookmarks UI using the real UI helper.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Tap the bookmarklet in the UI.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"TOCTOU_Bookmarklet")]
      performAction:grey_tap()];

  // Trigger background navigation programmatically immediately after tapping
  // the bookmarklet.
  GURL sensitiveURL = self.testServer->GetURL("/toctou_sensitive.html");
  [ChromeEarlGrey loadURL:sensitiveURL waitForCompletion:NO];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Tapping the bookmarklet should close the bookmarks UI.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];

  // Verify that the bookmarklet was NOT executed on the sensitive page.
  [ChromeEarlGrey waitForWebStateContainingText:"Not executed"];
}

// Tests that a bookmarklet is blocked if the active tab navigated to a
// different origin in the background while the Bookmarks UI was open, before
// the bookmarklet was tapped.
- (void)testBookmarkletTOCTOUMitigationWhileBookmarksOpen {
  // Add the bookmarklet programmatically.
  NSString* bookmarkletURL =
      @"javascript:document.getElementById('result').innerText='EXECUTED';";
  [BookmarkEarlGrey addBookmarkWithTitle:@"TOCTOU_Bookmarklet"
                                     URL:bookmarkletURL
                               inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Start the test server.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load the attacker page.
  GURL attackerURL = self.testServer->GetURL("/toctou_attacker.html");
  [ChromeEarlGrey loadURL:attackerURL];

  // Open Bookmarks UI using the real UI helper.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Trigger background navigation while the Bookmarks UI is open, BEFORE
  // tapping the bookmarklet.
  GURL sensitiveURL = self.testServer->GetURL("/toctou_sensitive.html");
  [ChromeEarlGrey loadURL:sensitiveURL waitForCompletion:NO];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Tap the bookmarklet in the UI.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"TOCTOU_Bookmarklet")]
      performAction:grey_tap()];

  // Tapping the bookmarklet should close the bookmarks UI.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];

  // Verify that the bookmarklet was NOT executed on the sensitive page.
  [ChromeEarlGrey waitForWebStateContainingText:"Not executed"];
}

// Tests that a bookmarklet executed during Bookmarks UI dismissal works
// successfully in the common case (where no background origin navigation
// occurs).
- (void)testBookmarkletExecutionInCommonCase {
  // Add the bookmarklet programmatically.
  NSString* bookmarkletURL =
      @"javascript:document.getElementById('result').innerText='EXECUTED';";
  [BookmarkEarlGrey addBookmarkWithTitle:@"TOCTOU_Bookmarklet"
                                     URL:bookmarkletURL
                               inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Start the test server.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load the sensitive page directly (which has no background navigation).
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/toctou_sensitive.html")];

  // Open Bookmarks UI using the real UI helper.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Tap the bookmarklet in the UI.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"TOCTOU_Bookmarklet")]
      performAction:grey_tap()];

  // Tapping the bookmarklet should close the bookmarks UI.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];

  // Verify that the bookmarklet WAS executed successfully on the page.
  [ChromeEarlGrey waitForWebStateContainingText:"EXECUTED"];
}

// Tests that a bookmarklet executed during Bookmarks UI dismissal works
// successfully on a fresh blank tab (where the last committed URL is empty).
- (void)testBookmarkletExecutionOnBlankTab {
  // Add the bookmarklet programmatically.
  NSString* bookmarkletURL =
      @"javascript:document.getElementById('result').innerText='EXECUTED';";
  [BookmarkEarlGrey addBookmarkWithTitle:@"TOCTOU_Bookmarklet"
                                     URL:bookmarkletURL
                               inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Open a fresh new blank tab.
  [ChromeEarlGrey openNewTab];

  // Open Bookmarks UI using the real UI helper.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Tap the bookmarklet in the UI.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"TOCTOU_Bookmarklet")]
      performAction:grey_tap()];

  // Tapping the bookmarklet should close the bookmarks UI successfully.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];
}

// Tests that opening a bookmark when there are no active tabs (e.g. all tabs
// closed) does not crash the app, and opens the bookmark successfully in a new
// tab.
- (void)testOpenBookmarkWithNoTabs {
  // Add a bookmark programmatically.
  [BookmarkEarlGrey addBookmarkWithTitle:@"TestBookmark"
                                     URL:@"about:blank"
                               inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Close all tabs programmatically.
  [ChromeEarlGrey closeAllTabs];

  // Start Bookmarks UI programmatically.
  [ChromeCoordinatorAppInterface startBookmarksCoordinator];

  // Open Mobile Bookmarks folder.
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Tap the bookmark.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"TestBookmark")]
      performAction:grey_tap()];

  // Tapping the bookmark should close the bookmarks UI.
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      grey_accessibilityID(kBookmarksHomeTableViewIdentifier)];

  // Verify that a tab was opened with the bookmark's URL.
  [ChromeEarlGrey waitForMainTabCount:1];
}

@end
