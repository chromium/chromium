// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/build_config.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

// Redefine EarlGrey macro to use line number and file name taken from the place
// of BookmarkEarlGreyUIImpl macro instantiation, rather than local line number
// inside test helper method. Original EarlGrey macro definition also expands to
// EarlGreyImpl instantiation. [self earlGrey] is provided by a superclass and
// returns EarlGreyImpl object created with correct line number and filename.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define EarlGrey [self earlGrey]
#pragma clang diagnostic pop

using chrome_test_util::BookmarksDestinationButton;
using chrome_test_util::BookmarksSaveEditDoneButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextBarTrailingButtonWithLabel;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::CopyLinkButton;
using chrome_test_util::DeleteButton;
using chrome_test_util::EditButton;
using chrome_test_util::MoveButton;
using chrome_test_util::OpenLinkInIncognitoButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::ShareButton;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

namespace chrome_test_util {

// Returns the label for the folder entry in the bookmark/folder editor.
NSString* FolderLabel(NSString* folderName, chrome_test_util::KindOfTest kind) {
  switch (kind) {
    case chrome_test_util::KindOfTest::kLocal:
      return l10n_util::GetNSStringF(
          IDS_IOS_BOOKMARKS_FOLDER_NAME_WITH_CLOUD_SLASH_ICON_LABEL,
          base::SysNSStringToUTF16(folderName));
    case chrome_test_util::KindOfTest::kAccount:
    case chrome_test_util::KindOfTest::kSignedOut:
      return folderName;
  }
}

id<GREYMatcher> BookmarksContextMenuEditButton() {
  // Making sure the edit button we're selecting is not on the bottom bar via
  // exclusion by accessibility ID and ancestry.
  return grey_allOf(
      EditButton(), grey_userInteractionEnabled(),
      grey_not(grey_accessibilityID(kBookmarksHomeTrailingButtonIdentifier)),
      grey_not(TabGridEditButton()),
      grey_not(grey_ancestor(
          grey_accessibilityID(kBookmarksHomeTrailingButtonIdentifier))),
      nil);
}

id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

id<GREYMatcher> BookmarksHomeDoneButton() {
  return grey_accessibilityID(kBookmarksHomeNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditDoneButton() {
  return grey_accessibilityID(kBookmarkEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditFolderButton() {
  return grey_accessibilityID(
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarksHomeLeadingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarksHomeCenterButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label) {
  return grey_allOf(
      grey_accessibilityID(kBookmarksHomeTrailingButtonIdentifier),
      grey_accessibilityLabel(label),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(label), grey_sufficientlyVisible(),
                    nil);
}

id<GREYMatcher> TappableBookmarkNodeWithLabel(
    NSString* label,
    chrome_test_util::KindOfTest kindOfTest) {
  NSString* accessibilityLabel;
  switch (kindOfTest) {
    case chrome_test_util::KindOfTest::kSignedOut:
    case chrome_test_util::KindOfTest::kAccount:
      accessibilityLabel = label;
      break;
    case chrome_test_util::KindOfTest::kLocal:
      accessibilityLabel =
          [NSString stringWithFormat:@"%@. Only on this device.", label];
      break;
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kBookmarksHomeSearchBarIdentifier);
}

}  // namespace chrome_test_util

@implementation BookmarkEarlGreyUIImpl

- (void)openBookmarks {
  // Opens the bookmark manager.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:BookmarksDestinationButton()];

  // Assert the menu is gone.
  [[EarlGrey selectElementWithMatcher:BookmarksDestinationButton()]
      assertWithMatcher:grey_nil()];
}

- (void)openBookmarksInWindowWithNumber:(int)windowNumber {
  // Opens the bookmark manager.
  [ChromeEarlGreyUI openToolsMenuInWindowWithNumber:windowNumber];
  [ChromeEarlGreyUI tapToolsMenuButton:BookmarksDestinationButton()];

  // Assert the menu is gone.
  [[EarlGrey selectElementWithMatcher:BookmarksDestinationButton()]
      assertWithMatcher:grey_nil()];
}

- (void)openMobileBookmarks:(chrome_test_util::KindOfTest)kindOfTest {
  NSString* label;
  switch (kindOfTest) {
    case chrome_test_util::KindOfTest::kSignedOut:
    case chrome_test_util::KindOfTest::kAccount:
      label = @"Mobile Bookmarks";
      break;
    case chrome_test_util::KindOfTest::kLocal:
      label = @"Mobile Bookmarks. Only on this device.";
      break;
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

- (void)openMobileBookmarks {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(
                                       grey_text(@"Mobile Bookmarks")),
                                   nil)] performAction:grey_tap()];
}

- (void)starCurrentTab {
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuAddToBookmarks),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];
}

- (void)addFolderWithName:(NSString*)name
                inStorage:(BookmarkStorageType)storageType {
  // Wait for folder picker to appear.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Create New Folder."
  NSString* accessibilityId =
      (storageType == BookmarkStorageType::kLocalOrSyncable)
          ? kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier
          : kBookmarkCreateNewAccountFolderCellIdentifier;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(accessibilityId)]
      performAction:grey_tap()];

  // Verify the folder creator is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Change the name of the folder.
  if (name.length > 0) {
    [[EarlGrey
        selectElementWithMatcher:[self
                                     textFieldMatcherForID:@"Title_textField"]]
        performAction:grey_replaceText(name)];
  }

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

- (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(title)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10),
                                                          condition),
             @"Waiting for bookmark to go away");
}

- (void)closeUndoSnackbarAndWait {
  id<GREYMatcher> snackbar_matcher =
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier");
  [[EarlGrey selectElementWithMatcher:snackbar_matcher]
      performAction:grey_tap()];
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  EG_TEST_HELPER_ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
                                 base::Seconds(10), condition),
                             @"Waiting for undo toast to go away");
}

- (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(folderTitle)];
}

- (void)closeContextBarEditMode {
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      performAction:grey_tap()];
}

- (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId {
  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Open All.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(buttonLabelId)]
      performAction:grey_tap()];
}

- (void)verifyContextMenuForSingleURLWithEditEnabled:(BOOL)editEnabled {
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CopyLinkButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Some actions need to be disabled when users cannot edit a given bookmark.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      assertWithMatcher:matcher];
  [[EarlGrey selectElementWithMatcher:DeleteButton()]
      assertWithMatcher:matcher];
}

- (void)verifyContextMenuForSingleFolderWithEditEnabled:(BOOL)editEnabled {
  // Edit and Move need to be disabled when users cannot edit a given
  // bookmark.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      assertWithMatcher:matcher];
  [[EarlGrey selectElementWithMatcher:MoveButton()] assertWithMatcher:matcher];
}

- (void)dismissContextMenu {
  // Since there are is no cancel action on the iOS 13 context menus, dismiss
  // by tapping elsewhere (on the key window).
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];
}

- (void)verifyActionSheetsForSingleURLWithEditEnabled:(BOOL)editEnabled {
  // Verify it shows the action sheets.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on the action sheets..
  // Verify that the edit menu option is enabled/disabled according to
  // `editEnabled`.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:matcher];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)verifyActionSheetsForSingleFolderWithEditEnabled:(BOOL)editEnabled {
  // Verify it shows the action sheets.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on the action sheets.
  // Verify that the edit menu option is enabled/disabled according to
  // `editEnabled`.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      assertWithMatcher:matcher];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:matcher];
}

- (void)dismissActionSheets {
  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the context menu. On non compact width tap the Bookmarks TableView
  // to dismiss, since there might not be a cancel button.
  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kBookmarksHomeTableViewIdentifier)]
        performAction:grey_tap()];
  }
}

- (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify context bar shows enabled "New Folder" and enabled "Select".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUI
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   newFolderEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarSelectString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   selectEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)verifyContextBarInEditMode {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyFolderFlowIsClosed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

- (void)verifyEmptyBackgroundAppears {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewIllustratedEmptyViewID)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_BOOKMARK_EMPTY_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // First make sure the empty message is visible at all, so the user knows it
  // exists.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_BOOKMARK_EMPTY_MESSAGE))]
      assertWithMatcher:grey_minimumVisiblePercent(0.25)];

  // Then make sure that scrolling the bookmark table view makes the empty
  // message sufficiently visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(l10n_util::GetNSString(
                                              IDS_IOS_BOOKMARK_EMPTY_MESSAGE)),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kBookmarksHomeTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyEmptyBackgroundIsAbsent {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewIllustratedEmptyViewID)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_BOOKMARK_EMPTY_TITLE))]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_BOOKMARK_EMPTY_MESSAGE))]
      assertWithMatcher:grey_nil()];
}

- (void)verifyEmptyState {
  [self verifyEmptyBackgroundAppears];

  // The search bar should not be visible when the illustrated empty state is
  // shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityTrait(
                                          UIAccessibilityTraitSearchField)]
      assertWithMatcher:grey_nil()];
}

- (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

- (void)scrollToBottom {
  // Provide a start points since it prevents some tests timing out under
  // certain configurations.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeTableViewIdentifier)]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeBottom, 0.5, 0.5)];
}

- (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle {
  // scroll to bottom to make sure new folder appears.
  [BookmarkEarlGreyUI scrollToBottom];
  // verify the folder is created.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      assertWithMatcher:grey_notNil()];
  // verify the editable textfield is gone.
  [[EarlGrey selectElementWithMatcher:
                 [self textFieldMatcherForID:@"bookmark_editing_text"]]
      assertWithMatcher:grey_notVisible()];
}

- (void)openFolderPicker {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

- (void)assertChangeFolderIsCorrectlySet:(NSString*)parentName
                              kindOfTest:
                                  (chrome_test_util::KindOfTest)kindOfTest {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(
                                       FolderLabel(parentName, kindOfTest)),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder
                    kindOfTest:(chrome_test_util::KindOfTest)kindOfTest {
  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the edit page (editor) is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Verify current parent folder for is correct.
  [self assertChangeFolderIsCorrectlySet:sourceFolder kindOfTest:kindOfTest];

  [BookmarkEarlGreyUI openFolderPicker];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the new destination folder. Use grey_ancestor since
  // BookmarksHomeTableView might be visible on the background on non-compact
  // widthts, and there might be a "destinationFolder" node there as well.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(TappableBookmarkNodeWithLabel(destinationFolder),
                            grey_ancestor(grey_accessibilityID(
                                kBookmarkFolderPickerViewContainerIdentifier)),
                            nil)] performAction:grey_tap()];

  // Verify folder picker is dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify parent folder has been changed in edit page.
  [self assertChangeFolderIsCorrectlySet:destinationFolder
                              kindOfTest:kindOfTest];

  // Dismiss edit page (editor).
  id<GREYMatcher> dismissMatcher = BookmarksSaveEditDoneButton();
  // If a folder is being edited use the EditFolder button dismiss matcher
  // instead.
  if ([editorId isEqualToString:kBookmarkFolderEditViewContainerIdentifier]) {
    dismissMatcher = BookmarksSaveEditFolderButton();
  }
  [[EarlGrey selectElementWithMatcher:dismissMatcher] performAction:grey_tap()];

  // Verify the Editor was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];

  [BookmarkEarlGreyUI closeUndoSnackbarAndWait];
}

- (void)tapOnLongPressContextMenuButton:(id<GREYMatcher>)actionMatcher
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through item context menu.
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:actionMatcher] performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:[self textFieldMatcherForID:textFieldId]]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through More... menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:[self textFieldMatcherForID:textFieldId]]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

- (NSString*)contextBarNewFolderString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
}

- (NSString*)contextBarDeleteString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
}

- (NSString*)contextBarCancelString {
  return l10n_util::GetNSString(IDS_CANCEL);
}

- (NSString*)contextBarSelectString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
}

- (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}

- (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn {
  // Click on "New Folder".
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeLeadingButtonIdentifier)]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"bookmark_editing_text";

  // Type the folder title, tapping to provide focus first so that we can \n
  // later.
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(folderTitle)];

  // Press the keyboard return key.
  if (pressReturn) {
    // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
    // replaceText can properly handle \n.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

    // Wait until the editing textfield is gone.
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey
          selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
          assertWithMatcher:grey_notVisible()
                      error:&error];
      return error == nil;
    };
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10),
                                                            condition),
               @"Waiting for textfield to go away");
  }
}

- (void)bookmarkCurrentTabWithTitle:(NSString*)title {
  [BookmarkEarlGreyUI starCurrentTab];

  // Set the bookmark name.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                          grey_not(TabGridEditButton()),
                                          ButtonWithAccessibilityLabelId(
                                              IDS_IOS_BOOKMARK_ACTION_EDIT),
                                          nil)] performAction:grey_tap()];

  NSString* titleIdentifier = @"Title Field_textField";
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(title)];

  // Dismiss the window.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Helpers

- (id<GREYMatcher>)textFieldMatcherForID:(NSString*)accessibilityID {
  return grey_allOf(grey_accessibilityID(accessibilityID),
                    grey_kindOfClassName(@"UITextField"),
                    grey_sufficientlyVisible(), nil);
}

@end
