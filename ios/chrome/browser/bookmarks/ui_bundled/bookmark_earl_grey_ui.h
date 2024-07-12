// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

enum class BookmarkStorageType;

@protocol GREYMatcher;

// Public macro to invoke helper methods in test methods (Test Process). Usage
// example:
//
// @interface PageLoadTestCase : XCTestCase
// @end
// @implementation PageLoadTestCase
// - (void)testPageload {
//   [BookmarkEarlGreyUIImpl loadURL:GURL("https://chromium.org")];
// }
//
// In this example BookmarkEarlGreyUIImpl must implement -loadURL:.
//
#define BookmarkEarlGreyUI \
  [BookmarkEarlGreyUIImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

namespace chrome_test_util {

// Represents in which state we expect to find the bookmark ui.
enum class KindOfTest {
  // The user is signed-out. All bookmarks are in the localOrSyncable model.
  kSignedOut,
  // The user is signed-in. The test consider the bookmarks in the
  // localOrSyncable model.
  kLocal,
  // The user is signed-in. The test consider the bookmarks in the account
  // model.
  kAccount
};

// Matcher for bookmarks tool tip star. (used in iPad)
id<GREYMatcher> StarButton();

// Matcher for the Edit button in the Bookmark context menu.
id<GREYMatcher> BookmarksContextMenuEditButton();

// Matcher for the Delete button on the bookmarks UI.
id<GREYMatcher> BookmarksDeleteSwipeButton();

// Matcher for the DONE button on the bookmarks UI.
id<GREYMatcher> BookmarksHomeDoneButton();

// Matcher for the DONE button on the bookmarks edit UI.
id<GREYMatcher> BookmarksSaveEditDoneButton();

// Matcher for the DONE button on the bookmarks edit folder UI.
id<GREYMatcher> BookmarksSaveEditFolderButton();

// Matcher for context bar leading button.
id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label);

// Matcher for context bar center button.
id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label);

// Matcher for context bar trailing button.
id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label);

// Matcher for tappable bookmark node. The second method must be used if the
// folder appear in both storage types.
id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label);
id<GREYMatcher> TappableBookmarkNodeWithLabel(
    NSString* label,
    chrome_test_util::KindOfTest kindOfTest);

// Matcher for the search button.
id<GREYMatcher> SearchIconButton();

}  // namespace chrome_test_util

// Test methods that perform actions on Bookmarks. These methods only affect
// Chrome using the UI with Earl Grey. Used for logging the failure. Compiled in
// Test Process for EG2 and EG1.
@interface BookmarkEarlGreyUIImpl : BaseEGTestHelperImpl

// Navigates to the bookmark manager UI.
- (void)openBookmarks;

// Navigates to the bookmark manager UI in window with number `windowNumber`.
// Sets and Leaves the root matcher to the given window.
- (void)openBookmarksInWindowWithNumber:(int)windowNumber;

// Selects MobileBookmarks to open. The second method is used in case of
// ambiguity.
- (void)openMobileBookmarks;
- (void)openMobileBookmarks:(chrome_test_util::KindOfTest)kindOfTest;

// Adds a bookmark for the current tab. Must be called when on a tab.
- (void)starCurrentTab;

// Creates a new folder in `storageType` starting from the folder picker.
// Passing a `name` of 0 length will use the default value.
- (void)addFolderWithName:(NSString*)name
                inStorage:(BookmarkStorageType)storageType;

// Waits for the disparition of the given `title` in the UI.
- (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title;

// Tap on the snackbar to make it go away.
- (void)closeUndoSnackbarAndWait;

// Rename folder title to `folderTitle`. Must be in edit folder UI.
- (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle;

// Close edit mode.
- (void)closeContextBarEditMode;

// Select urls from Mobile Bookmarks and tap on a specified context bar button.
// Must be called after previously calling [BookmarkEarlGreyUI openBookmarks].
- (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId;

- (void)verifyContextMenuForSingleURLWithEditEnabled:(BOOL)editEnabled;

- (void)verifyContextMenuForSingleFolderWithEditEnabled:(BOOL)editEnabled;

- (void)dismissContextMenu;

- (void)verifyActionSheetsForSingleURLWithEditEnabled:(BOOL)editEnabled;

- (void)verifyActionSheetsForSingleFolderWithEditEnabled:(BOOL)editEnabled;

- (void)dismissActionSheets;

- (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled;

- (void)verifyContextBarInEditMode;

- (void)verifyFolderFlowIsClosed;

- (void)verifyEmptyBackgroundAppears;

- (void)verifyEmptyBackgroundIsAbsent;

- (void)verifyEmptyState;

- (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder;

// Scroll the bookmarks to bottom.
- (void)scrollToBottom;

// Verify a folder with given name is created and it is not being edited.
- (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle;

// Opens the folder picker. This method should be called from the bookmark or
// the folder editor.
- (void)openFolderPicker;

// Checks that, in the bookmark or folder editor, the currently edited object is
// in the correct folder. The name of the folder owning the edited node is
// `parentName`. The expected label of the folder depends on `kindOfTest`.
- (void)assertChangeFolderIsCorrectlySet:(NSString*)parentName
                              kindOfTest:
                                  (chrome_test_util::KindOfTest)kindOfTest;

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder
                    kindOfTest:(chrome_test_util::KindOfTest)kindOfTest;

- (void)tapOnLongPressContextMenuButton:(id<GREYMatcher>)actionMatcher
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId;

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId;

// Context bar strings.
- (NSString*)contextBarNewFolderString;

- (NSString*)contextBarDeleteString;

- (NSString*)contextBarCancelString;

- (NSString*)contextBarSelectString;

- (NSString*)contextBarMoreString;

// Create a new folder with given title.
- (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn;

// Bookmarks the current tab using `title` as Bookmark title. Make sure the
// Bookmark loaded is loaded before by calling [BookmarkEarlGrey
// waitForBookmarkModelLoaded];
- (void)bookmarkCurrentTabWithTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_EARL_GREY_UI_H_
