// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/public/bookmarks_ui_constants.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/test/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/net_errors.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::BookmarksSaveEditDoneButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

// Bookmark integration tests for Chrome focused on its interactions with the
// rest of Chrome.
@interface BookmarksInteractionTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksInteractionTestCase

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDownHelper {
  [super tearDownHelper];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

#pragma mark - BookmarksTestCase Tests

// Verifies that adding a bookmark and removing a bookmark via the UI properly
// updates the BookmarkModel.
- (void)testAddRemoveBookmark {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  // Add the bookmark from the UI.
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGreyUI bookmarkCurrentTabWithTitle:bookmarkTitle];

  // Verify the bookmark is set.
  [BookmarkEarlGrey
      verifyBookmarksWithTitle:bookmarkTitle
                 expectedCount:1
                     inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Open the BookmarkEditor.

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuEditBookmark),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];

  // Delete the Bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Verify the bookmark is not in the BookmarkModel.
  [BookmarkEarlGrey
      verifyBookmarksWithTitle:bookmarkTitle
                 expectedCount:0
                     inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Verify the the page is no longer bookmarked.

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuAddToBookmarks),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      assertWithMatcher:grey_notNil()];
  // After veryfing, close the ToolsMenu.
  [ChromeEarlGreyUI closeToolsMenu];

  // Close the opened tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Test to set bookmarks in multiple tabs.
- (void)testBookmarkMultipleTabs {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL firstURL = self.testServer->GetURL("/pony.html");
  const GURL secondURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:firstURL];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:secondURL];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGreyUI bookmarkCurrentTabWithTitle:@"my bookmark"];
  [BookmarkEarlGrey
      verifyBookmarksWithTitle:@"my bookmark"
                 expectedCount:1
                     inStorage:BookmarkStorageType::kLocalOrSyncable];
}

// Regression test for crbug.com/1426259.
// Tests that there is no crash when opening from incognito tab.
- (void)testOpeningBookmarksInIncognitoMode {
  [ChromeEarlGrey openNewIncognitoTab];

  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
}

// Tests opening the folder chooser from the bookmark editor using
// an incognito tab.
// See http://crbug.com/1432310.
- (void)testOpenFolderChooserFromBookmarkEditorWithIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
  // Invoke Edit through long press on "First URL" bookmark.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];
  // Tap the Folder button.
  [BookmarkEarlGreyUI openFolderPicker];
  // Close the folder chooser, bookmark editor and the bookmark list.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(BookmarksNavigationBarBackButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];
}

// Tests opening the folder chooser from the folder editor using
// an incognito tab.
// See http://crbug.com/1432310.
- (void)testOpenFolderChooserFromFolderEditorWithIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
  // Invoke Edit through long press on "Folder 1" folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];
  // Tap the Folder button.
  [BookmarkEarlGreyUI openFolderPicker];
  // Close the folder chooser, bookmark editor and the bookmark list.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(BookmarksNavigationBarBackButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];
}

// Tests that keyboard commands are registered when a bookmark is added as it
// shows only a snackbar.
- (void)testKeyboardCommandsRegistered_AddBookmark {
  // Add the bookmark.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL firstURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:firstURL];
  [BookmarkEarlGreyUI starCurrentTab];
  GREYAssertTrue([ChromeEarlGrey registeredKeyCommandCount] > 0,
                 @"Some keyboard commands are registered.");
}

// Tests that keyboard commands are not registered when a bookmark is edited, as
// the edit screen is presented modally.
- (void)testKeyboardCommandsNotRegistered_EditBookmark {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuEditBookmark),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];

  GREYAssertTrue([ChromeEarlGrey registeredKeyCommandCount] == 0,
                 @"No keyboard commands are registered.");
}

// Tests that chrome://bookmarks is disabled.
- (void)testBookmarksURLDisabled {
  const std::string kChromeBookmarksURL = "chrome://bookmarks/";
  [ChromeEarlGrey loadURL:GURL(kChromeBookmarksURL)];

  // Verify chrome://bookmarks is the loaded URL.
  [ChromeEarlGrey waitForWebStateVisibleURL:GURL(kChromeBookmarksURL)];

  // Verify that the resulting page is an error page.
  std::string errorMessage = net::ErrorToShortString(net::ERR_INVALID_URL);
  [ChromeEarlGrey waitForWebStateContainingText:errorMessage];
}

// Tests that bookmarking an incognito tab actually bookmarks the
// expected URL. Regression test for https://crbug.com/1353114.
- (void)testBookmarkFromIncognito {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL firstURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:firstURL];

  const GURL incognitoURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:incognitoURL];

  // Add the bookmark from the UI.
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  NSString* bookmarkTitle = @"Test Page";
  [BookmarkEarlGreyUI bookmarkCurrentTabWithTitle:@"Test Page"];

  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:base::SysUTF8ToNSString(
                                           incognitoURL.spec())
                                  name:bookmarkTitle
                             inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:base::SysUTF8ToNSString(firstURL.spec())
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
}

// Verifies that swiping down the bookmark editor dismisses the view only if the
// displayed URL is valid.
- (void)testSwipeDownBookmarkEditor {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [ChromeEarlGrey waitForWebStateVisibleURL:bookmarkedURL];

  // Add the bookmark from the UI.
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGreyUI bookmarkCurrentTabWithTitle:bookmarkTitle];

  // Verify the bookmark is set.
  [BookmarkEarlGrey
      verifyBookmarksWithTitle:bookmarkTitle
                 expectedCount:1
                     inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Open the BookmarkEditor.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuEditBookmark),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];

  // Open the BookmarkEditor.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuEditBookmark),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];

  // Remove displayed URL
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"URL Field")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"URL Field_textField"),
                                   grey_kindOfClassName(@"UITextField"),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_replaceText(@"")];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView is still presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests to open a bookmark from the account storage and then signs out.
// Related to crbug.com/421139931.
- (void)testOpenBookmarkFromAccountStorageAndSignout {
  // Adds and signs in with `fakeIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add one account bookmark.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];
  [ChromeEarlGreyUI waitForAppToIdle];

  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
  // Open a bookmark in current tab in a normal session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" is the current tab URL.
  [ChromeEarlGrey waitForWebStateVisibleURL:GetFirstUrl()];

  [SigninEarlGreyUI signOut];
  [SigninEarlGrey verifySignedOut];
}

// TODO(crbug.com/40508042): Add egtests for:
// 1. Spinner background.
// 2. Reorder bookmarks. (make sure it won't clear the row selection on table)
// 3. Test new folder name is committed when name editing is interrupted by
//    tapping context bar buttons.

@end
