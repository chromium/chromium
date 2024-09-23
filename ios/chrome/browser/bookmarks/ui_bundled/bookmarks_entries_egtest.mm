// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/build_config.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"

using chrome_test_util::BookmarksContextMenuEditButton;
using chrome_test_util::BookmarksDeleteSwipeButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::OpenLinkInIncognitoButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::OpenLinkInNewWindowButton;
using chrome_test_util::SwipeToShowDeleteButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;
using chrome_test_util::WindowWithNumber;

namespace {
constexpr char kURL1[] = "http://firstURL";
constexpr char kTitle1[] = "Page 1";
constexpr char kResponse1[] = "Test Page 1 content";
constexpr char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";

// Matcher for the add bookmark button in the tools menu.
id<GREYMatcher> AddBookmarkButton() {
  return grey_accessibilityID(kToolsMenuAddToBookmarks);
}
}  // namespace

// Bookmark entries integration tests for Chrome.
@interface BookmarksEntriesTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksEntriesTestCase

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tear down called once per test.
- (void)tearDown {
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  [super tearDown];
}

#pragma mark - BookmarksEntriesTestCase Tests

- (void)testUndoDeleteBookmarkFromSwipe {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:SwipeToShowDeleteButton()];

  // Verify context bar does not change when "Delete" shows up.
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
  // Delete it.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify context bar remains in default state.
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testSwipeToDeleteDisabledInEditMode {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:SwipeToShowDeleteButton()];

  // Verify the delete confirmation button shows up.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify the delete confirmation button is gone after entering edit mode.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_nil()];

  // Swipe action on "Second URL".  This should not bring out delete
  // confirmation button as swipe-to-delete is disabled in edit mode.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:SwipeToShowDeleteButton()];

  // Verify the delete confirmation button doesn't appear.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_nil()];

  // Cancel edit mode
  [BookmarkEarlGreyUI closeContextBarEditMode];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      performAction:SwipeToShowDeleteButton()];

  // Verify the delete confirmation button shows up. (swipe-to-delete is
  // re-enabled).
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_notNil()];
}

- (void)testActionSheetsForSingleURLSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Tap More...
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarkEarlGreyUI verifyActionSheetsForSingleURLWithEditEnabled:YES];
}

// Verify Edit Text functionality on single URL selection.
- (void)testEditTextOnSingleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // 1. Edit the bookmark title at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];

  // Modify the title.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"Title Field_textField"
                          to:@"n5"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Verify that the bookmark was updated.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"n5")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Press undo and verify old URL is back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // 2. Edit the bookmark url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Modify the url.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"URL Field_textField"
                          to:@"www.b.fr"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Verify that the bookmark was updated.
  [BookmarkEarlGrey
      verifyExistenceOfBookmarkWithURL:@"http://www.b.fr/"
                                  name:@"French URL"
                             inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Press undo and verify the edit is undone.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:@"http://www.b.fr/"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

// Verify Move functionality on single URL selection.
- (void)testMoveOnSingleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // 1. Move a single url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single url.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Move the "Second URL" to "Folder 1.1".
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
           setParentFolderTo:@"Folder 1.1"
                        from:@"Mobile Bookmarks"
                  kindOfTest:chrome_test_util::KindOfTest::kSignedOut];

  // Verify edit mode remains.
  [BookmarkEarlGreyUI verifyContextBarInEditMode];

  // Close edit mode.
  [BookmarkEarlGreyUI closeContextBarEditMode];

  // Navigate to "Folder 1.1" and verify "Second URL" is under it.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // 2. Test the cancel button at edit page.

  // Come back to the Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"French URL"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap cancel after modifying the url.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"URL Field_textField"
                          to:@"www.b.fr"
                 dismissWith:@"Cancel"];

  // Verify that the bookmark was not updated.
  [BookmarkEarlGrey
      verifyAbsenceOfBookmarkWithURL:@"http://www.b.fr/"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Verify edit mode remains.
  [BookmarkEarlGreyUI verifyContextBarInEditMode];
}

// Verify Copy URL functionality on single URL selection.
- (void)testCopyFunctionalityOnSingleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Invoke Copy through context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Select Copy URL.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      performAction:grey_tap()];

  // Verify general pasteboard has the URL copied.
  [ChromeEarlGrey verifyStringCopied:@"www.a.fr"];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testContextMenuForMultipleURLSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify the Open All functionality on multiple url selection.
- (void)testContextMenuForMultipleURLOpenAll {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open 3 normal tabs from a normal session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 3 normal tabs.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  // Switch to Incognito mode by adding a new incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];

  [BookmarkEarlGreyUI openBookmarks];

  // Open 3 normal tabs from a incognito session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 6 normal tabs and no new incognito tabs.
  [ChromeEarlGrey waitForMainTabCount:6];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 1,
                 @"Incognito tab count should be 1");

  // Close the incognito tab to go back to normal mode.
  [ChromeEarlGrey closeAllIncognitoTabs];

  // The following verifies the selected bookmarks are open in the same order as
  // in folder.

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:3];
}

// Verify the Open All in Incognito functionality on multiple url selection.
- (void)testContextMenuForMultipleURLOpenAllInIncognito {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open 3 incognito tabs from a normal session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // Verify there are 3 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:3];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  [BookmarkEarlGreyUI openBookmarks];

  // Open 3 incognito tabs from a incognito session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // The 3rd tab will be re-used to open one of the selected bookmarks.  So
  // there will be 2 new tabs only.

  // Verify there are 5 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:5];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:2];
}

// Verify the Open and Open in Incognito functionality on single url.
- (void)testOpenSingleBookmarkInNormalAndIncognitoTab {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open a bookmark in current tab in a normal session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in new tab from a normal session (through a long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  // Verify there is 1 new normal tab created and no new incognito tab created.
  [ChromeEarlGrey waitForMainTabCount:2];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in an incognito tab from a normal session (through a long
  // press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton()]
      performAction:grey_tap()];

  // Verify there is 1 incognito tab created and no new normal tab created.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in current tab from a incognito session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to staying at incognito mode");

  // Verify no new tabs created.
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 1,
                 @"Incognito tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in new incognito tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton()]
      performAction:grey_tap()];

  // Verify a new incognito tab is created.
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to staying at incognito mode");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in a new normal tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  // Verify a new normal tab is created and no incognito tab is created.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 2,
                 @"Incognito tab count should be 2");

  // Verify the current tab is a normal tab.
  GREYAssertFalse([ChromeEarlGrey isIncognitoMode],
                  @"Failed to switch to normal mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForMixedSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testLongPressOnSingleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  // Verify context menu.
  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:YES];
}

// Verify Move functionality on mixed folder / url selection.
- (void)testMoveFunctionalityOnMixedSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Verify folder picker appeared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Delete the First URL programmatically in background.  Folder picker will
  // not close as the selected nodes "Second URL" and "Folder 1" still exist.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"First URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Choose to move into a new folder.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarkEarlGreyUI
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.

  [BookmarkEarlGreyUI
      assertChangeFolderIsCorrectlySet:@"Mobile Bookmarks"
                            kindOfTest:chrome_test_util::KindOfTest::
                                           kSignedOut];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];

  [BookmarkEarlGreyUI closeUndoSnackbarAndWait];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // Verify new folder "Title For New Folder" has two bookmark nodes.
  [BookmarkEarlGrey verifyChildCount:2
                    inFolderWithName:@"Title For New Folder"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Drill down to where "Second URL" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple url selection.
- (void)testMoveFunctionalityOnMultipleUrlSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into Folder 1. Use grey_ancestor since
  // BookmarksHomeTableView might be visible on the background on non-compact
  // widthts, and there might be a "Folder1" node there as well.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(TappableBookmarkNodeWithLabel(@"Folder 1"),
                            grey_ancestor(grey_accessibilityID(
                                kBookmarkFolderPickerViewContainerIdentifier)),
                            nil)] performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];

  [BookmarkEarlGreyUI closeUndoSnackbarAndWait];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // Verify Folder 1 has three bookmark nodes.
  [BookmarkEarlGrey verifyChildCount:3
                    inFolderWithName:@"Folder 1"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Drill down to where "Second URL" and "First URL" have been moved and assert
  // it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move is cancelled when all selected folder/url are deleted in
// background.
- (void)testMoveCancelledWhenAllSelectionDeleted {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Verify folder picker appeared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Delete the selected URL and folder programmatically.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Second URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Verify folder picker is exited.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditorDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10),
                                                          condition),
             @"Waiting for bookmark to go away");

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify Delete is disabled (with visible Delete, it also means edit mode is
  // stayed).
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)testDeleteSingleURLNode {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testDeleteMultipleNodes {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder and URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testSwipeDownToDismissFromPushedVC {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open Edit Bookmark through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Tap on Folder to open folder picker.
  [BookmarkEarlGreyUI openFolderPicker];

  // Check that Change Folder is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];

  // Open EditBookmark again to verify it was cleaned up successsfully.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Swipe EditBookmark down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the EditBookmark has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Multiwindow

// Tests display and selection of 'Open in New Window' in a context menu on a
// bookmarks entry.
- (void)testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [BookmarkEarlGrey clearBookmarksPositionCache];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [ChromeEarlGrey waitForForegroundWindowCount:1];

  // Open a bookmark in a new window (through a long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_longPress()];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:"pony jokes"];
}

- (void)testBookmarksSyncInMultiwindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // TODO(crbug.com/40210654).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  GURL URL1 = web::test::HttpServer::MakeUrl(kURL1);

  std::map<GURL, std::string> responses;
  responses[URL1] = base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  web::test::SetUpSimpleHttpServer(responses);

  [BookmarkEarlGrey clearBookmarksPositionCache];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  // Open bookmark panel in a second window
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [BookmarkEarlGreyUI openBookmarksInWindowWithNumber:1];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Load url in first window and bookmark it.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [ChromeEarlGrey loadURL:URL1 inWindowWithNumber:0];
  [ChromeEarlGreyUI openToolsMenuInWindowWithNumber:0];
  [ChromeEarlGreyUI tapToolsMenuButton:AddBookmarkButton()];

  // Assert it appeared in second window's list.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          base::SysUTF8ToNSString(kTitle1))]
      assertWithMatcher:grey_notNil()];

  // Open bookmark panel in first window also.
  [BookmarkEarlGreyUI openBookmarksInWindowWithNumber:0];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          base::SysUTF8ToNSString(kTitle1))]
      assertWithMatcher:grey_notNil()];

  // Delete item from first window.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          base::SysUTF8ToNSString(kTitle1))]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          base::SysUTF8ToNSString(kTitle1))]
      assertWithMatcher:grey_nil()];

  // And make sure it has disappeared from second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          base::SysUTF8ToNSString(kTitle1))]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Verifies the Mobile Bookmarks's urls are open in the same order as they are
// in folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex {
  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "Second URL" appears.
  // TODO(crbug.com/40508042): see we if can add switchToNextTab to
  // chrome_test_util so that we don't need to pass tabIndex here.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "First URL" appears.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
