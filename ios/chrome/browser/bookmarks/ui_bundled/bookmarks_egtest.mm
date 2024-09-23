// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/net_errors.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BookmarksHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::BookmarksSaveEditDoneButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextBarTrailingButtonWithLabel;
using chrome_test_util::OmniboxText;
using chrome_test_util::StarButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

// Bookmark integration tests for Chrome.
@interface BookmarksTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksTestCase

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

#pragma mark - BookmarksTestCase Tests

// Verifies that adding a bookmark and removing a bookmark via the UI properly
// updates the BookmarkModel.
- (void)testAddRemoveBookmark {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

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

// Test deleting grand parent is reflected in the bookmarks list UI. Regression
// test for crbug.com/1445457
- (void)testRemoveGrandParentFolder {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Enter Folder 1
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  // Enter Folder 2
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];
  // Enter Folder 3
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 3")]
      performAction:grey_tap()];

  // Delete the Folder 1 programmatically in background. This will delete child
  // Folder 2 and Folder 3 as well.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Ensure Folder 1.1 is seen, that means it successfully comes back to Mobile
  // Bookmarks.
  [BookmarkEarlGreyUI verifyBookmarkFolderIsSeen:@"Folder 1.1"];
}

// Test deleting grand parent is reflected in the bookmarks folder editor UI.
// Regression test for crbug.com/1446133
- (void)testRemoveGrandParentWhileEditingFolder {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Enter Folder 1
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  // Enter Folder 2
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];
  // Enter edit mode with Folder 3
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 3")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Delete the Folder 1 programmatically in background. This will delete child
  // Folder 2 and Folder 3 as well.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Ensure Folder 1.1 is seen, that means it successfully comes back to Mobile
  // Bookmarks.
  [BookmarkEarlGreyUI verifyBookmarkFolderIsSeen:@"Folder 1.1"];
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

// Tests that changes to the parent folder from the Single Bookmark Editor
// are saved to the bookmark only when saving the results.
- (void)testMoveDoesSaveOnSave {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Tap the Folder button.
  [BookmarkEarlGreyUI openFolderPicker];

  // Create a new folder.
  [BookmarkEarlGreyUI addFolderWithName:nil
                              inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Verify that the editor is present.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Check that the new folder doesn't contain the bookmark.
  [BookmarkEarlGrey verifyChildCount:0
                    inFolderWithName:@"New folder"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Check that the new folder contains the bookmark.
  [BookmarkEarlGrey verifyChildCount:1
                    inFolderWithName:@"New folder"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  // Check that the new folder still contains the bookmark.
  [BookmarkEarlGrey verifyChildCount:1
                    inFolderWithName:@"New folder"
                           inStorage:BookmarkStorageType::kLocalOrSyncable];
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

// Tests that the bookmark context bar is shown in MobileBookmarks.
- (void)testBookmarkContextBarShown {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify the context bar's leading and trailing buttons are shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeLeadingButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
}

- (void)testBookmarkContextBarInSingleSelectionModes {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Cancel edit mode
  [BookmarkEarlGreyUI closeContextBarEditMode];

  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testBookmarkContextBarInMultipleSelectionModes {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Multi select URL and folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect Folder 1, so that Second URL is selected.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all, but one Folder - Folder 1 is selected.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  // Unselect URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Cancel edit mode
  [BookmarkEarlGreyUI closeContextBarEditMode];

  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

// Tests when total height of bookmarks exceeds screen height.
- (void)testBookmarksExceedsScreenHeight {
  [BookmarkEarlGrey setupBookmarksWhichExceedsScreenHeightInStorage:
                        BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify bottom URL is not visible before scrolling to bottom (make sure
  // setupBookmarksWhichExceedsScreenHeight works as expected).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom URL")]
      assertWithMatcher:grey_notVisible()];

  // Verify the top URL is visible (isn't covered by the navigation bar).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Top URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Test new folder could be created.  This verifies bookmarks scrolled to
  // bottom successfully for folder name editng.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarkEarlGreyUI createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                 pressReturn:YES];
  [BookmarkEarlGreyUI verifyFolderCreatedWithTitle:newFolderTitle];
}

// Tests the new folder name is committed when "hide keyboard" button is
// pressed. (iPad specific)
- (void)testNewFolderNameCommittedWhenKeyboardDismissedOnIpad {
#if TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_SKIPPED(@"The keyboard is not considered 'dismissed' on "
                         @"simulator when tapping on 'hide keyboard'.");
#endif
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not supported on iPhone");
  }
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarkEarlGreyUI createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                 pressReturn:NO];

  // Tap on the "hide keyboard" button.
  id<GREYMatcher> hideKeyboard = grey_accessibilityLabel(@"Hide keyboard");
  [[EarlGrey selectElementWithMatcher:hideKeyboard] performAction:grey_tap()];

  // Tap on "New Folder 1".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"New Folder 1")]
      performAction:grey_tap()];

  // Verify the empty background appears. (If "New Folder 1" is commited,
  // tapping on it will enter it and see a empty background.  Instead of
  // re-editing it (crbug.com/794155)).
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

- (void)testEmptyBackgroundAndSelectButton {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Enter Folder 1.1 (which is empty)
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Verify the empty background appears.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];

  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select every URL and folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap delete on context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  [BookmarkEarlGreyUI closeUndoSnackbarAndWait];

  // Verify edit mode is close automatically (context bar switched back to
  // default state) and select button is disabled.
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                     newFolderEnabled:YES];

  // Verify the empty background appears.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

- (void)testCachePositionIsRecreated {
  [BookmarkEarlGrey setupBookmarksWhichExceedsScreenHeightInStorage:
                        BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Verify Bottom 1 is not visible before scrolling to bottom (make sure
  // setupBookmarksWhichExceedsScreenHeight works as expected).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom 1")]
      assertWithMatcher:grey_notVisible()];

  // Scroll to the bottom so that Bottom 1 is visible.
  [BookmarkEarlGreyUI scrollToBottom];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom 1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  // Reopen bookmarks.
  [BookmarkEarlGreyUI openBookmarks];

  // Ensure the Bottom 1 of Folder 1 is visible.  That means both folder and
  // scroll position are restored successfully.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"Bottom 1, google.com")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify root node is opened when cache position is deleted.
- (void)testCachePositionIsResetWhenNodeIsDeleted {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Select Folder 2.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Close bookmarks, it will store Folder 2 as the cache position.
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // Delete Folder 2.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 2"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Reopen bookmarks.
  [BookmarkEarlGreyUI openBookmarks];

  // Ensure the root node is opened, by verifying Mobile Bookmarks is seen in a
  // table cell.
  [BookmarkEarlGreyUI verifyBookmarkFolderIsSeen:@"Mobile Bookmarks"];
}

// Verify root node is opened when cache position is a permanent node and is
// empty.
- (void)testCachePositionIsResetWhenNodeIsPermanentAndEmpty {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Close bookmarks, it will store Mobile Bookmarks as the cache position.
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  // Delete all bookmarks and folders under Mobile Bookmarks.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1.1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"French URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Second URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"First URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Reopen bookmarks.
  [BookmarkEarlGreyUI openBookmarks];

  // Ensure the root node is opened, by verifying that there isn't a Back
  // button in the navigation bar.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksNavigationBarBackButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testCachePositionIsRecreatedWhenNodeIsMoved {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Select Folder 2.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Select Folder 3
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 3")]
      performAction:grey_tap()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksHomeDoneButton()]
      performAction:grey_tap()];

  // Move Folder 3 under Folder 1.
  [BookmarkEarlGrey
      moveBookmarkWithTitle:@"Folder 3"
          toFolderWithTitle:@"Folder 1"
                  inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Reopen bookmarks.
  [BookmarkEarlGreyUI openBookmarks];

  // Go back 1 level to Folder 1.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Ensure we are at Folder 1, by verifying folders at this level.
  [BookmarkEarlGreyUI verifyBookmarkFolderIsSeen:@"Folder 2"];
}

// Tests that chrome://bookmarks is disabled.
- (void)testBookmarksURLDisabled {
  const std::string kChromeBookmarksURL = "chrome://bookmarks";
  [ChromeEarlGrey loadURL:GURL(kChromeBookmarksURL)];

  // Verify chrome://bookmarks appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(kChromeBookmarksURL)]
      assertWithMatcher:grey_notNil()];

  // Verify that the resulting page is an error page.
  std::string errorMessage = net::ErrorToShortString(net::ERR_INVALID_URL);
  [ChromeEarlGrey waitForWebStateContainingText:errorMessage];
}

- (void)testSwipeDownToDismiss {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that swiping down dismisses multiple bookmarks UIs on the same
// navigation controller.
- (void)testSwipeDownToDismissMultipleBookmarksUI {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Tap the Folder button and verify folder chooser UI is displayed.
  [BookmarkEarlGreyUI openFolderPicker];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose to move the bookmark into a new folder and verify folder creator is
  // visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Folder button to open folder chooser UI and verify `New Folder`
  // section is not visible.
  [BookmarkEarlGreyUI openFolderPicker];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Swipe TableView down and verify that we're back to the bookmarks list.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testFolderEmptyState {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Enter Folder 1.1 (which is empty)
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Empty TableView background should be visible.
  [BookmarkEarlGreyUI verifyEmptyState];
}

// Test to make sure the Mobile Bookmarks folder is not created if empty.
- (void)testRootEmptyState {
  [BookmarkEarlGreyUI openBookmarks];

  // When the user has no bookmarks, the root view should be an empty state.
  [BookmarkEarlGreyUI verifyEmptyState];
}

// When deleting the last bookmark, the root view should be empty when
// navigating back.
- (void)testRootEmptyStateAfterAllBookmarkDeleted {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Delete all bookmarks and folders under Mobile Bookmarks.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1.1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"French URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Second URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"First URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Navigate back to the root view.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // When the user has no bookmarks, the root view should be an empty state.
  [BookmarkEarlGreyUI verifyEmptyState];
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

// Test that when bookmark is on edit mode and all entries are deleted outside
// of that window it automatically quits edit mode.
- (void)testBookmarksSyncWhenAllEntriesAreCancelled {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Go in edit mode.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Delete all bookmarks and folders under Mobile Bookmarks.
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1.1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Folder 1"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"French URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"Second URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Check window is still in edit mode (still one bookmark/folder left).
  [BookmarkEarlGreyUI verifyContextBarInEditMode];
  [BookmarkEarlGrey
      removeBookmarkWithTitle:@"First URL"
                    inStorage:BookmarkStorageType::kLocalOrSyncable];

  // Check window is no more in edit mode (no bookmark/folder left).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                     newFolderEnabled:YES];
}

// Test to swipe down the bookmark view twice.
- (void)testBookmarksSwipeDownTwice {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
  // Repeat a second time.
  [BookmarkEarlGreyUI openBookmarks];
  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Test to swipe down the bookmark view after opening a bookmark folder.
- (void)testBookmarksSwipeDownAfterOpeningBookmarkFolder {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];
  [BookmarkEarlGreyUI openMobileBookmarks];
  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Test to press back button while editing a folder name in the bookmark table
// view.
// Related to http://crbug.com/1466119.
- (void)testPressBackbuttonWhileEditingBookmarkName {
  // Open the bookmark view.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Click on "New folder".
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeLeadingButtonIdentifier)]
      performAction:grey_tap()];
  // Press the back button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(BookmarksNavigationBarBackButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

// Verifies that swiping down the bookmark editor dismisses the view only if the
// displayed URL is valid.
- (void)testSwipeDownBookmarkEditor {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  const GURL bookmarkedURL = self.testServer->GetURL("/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

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

// TODO(crbug.com/40508042): Add egtests for:
// 1. Spinner background.
// 2. Reorder bookmarks. (make sure it won't clear the row selection on table)
// 3. Test new folder name is committed when name editing is interrupted by
//    tapping context bar buttons.

@end
