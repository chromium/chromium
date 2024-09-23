// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::BookmarksDeleteSwipeButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextBarTrailingButtonWithLabel;
using chrome_test_util::SearchIconButton;
using chrome_test_util::SwipeToShowDeleteButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

// Bookmark search integration tests for Chrome.
@interface BookmarksSearchTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksSearchTestCase

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

#pragma mark - BookmarksSearchTestCase Tests

// Tests that the search bar is shown on root.
- (void)testSearchBarShownOnRoot {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Verify the search bar is shown.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests that the search bar is shown on mobile list.
- (void)testSearchBarShownOnMobileBookmarks {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify the search bar is shown.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   grey_sufficientlyVisible(), nil)];
}

// Tests the search.
- (void)testSearchResults {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify we have our 3 items.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_notNil()];

  // Search 'o', tapping first to provide focus.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"o")];

  // Verify that folders are not filtered out.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Search 'on'.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"n" flags:0];

  // Verify we are left only with the "First" and "Second" one.
  // 'on' matches 'pony.html' and 'Second'
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_nil()];
  // Verify that folders are not filtered out.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      assertWithMatcher:grey_nil()];

  // Search again for 'ony'.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"y" flags:0];

  // Verify we are left only with the "First" one for 'pony.html'.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_nil()];
}

// Tests that you get 'No Results' when no matching bookmarks are found.
- (void)testSearchWithNoResults {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search 'zz', tapping first to provide focus.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"zz")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Verify that we have a 'No Results' label somewhere.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_HISTORY_NO_SEARCH_RESULTS))]
      assertWithMatcher:grey_notNil()];

  // Verify that Edit button is disabled.
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarSelectString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Tests that scrim is shown while search box is enabled with no queries.
- (void)testSearchScrimShownWhenSearchBoxEnabled {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Searching.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"i")];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Go back to original folder content.
  // TODO(crbug.com/40916973): Revert to grey_clearText when fixed in EG.
  // (grey_replaceText(@""))
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"")];

  // Verify that scrim is visible again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Cancel.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping scrim while search box is enabled dismisses the search
// controller.
- (void)testSearchTapOnScrimCancelsSearchController {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Tap on scrim.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      performAction:grey_tap()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Verifiy we went back to original folder content.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_notNil()];
}

// Tests that long press on scrim while search box is enabled dismisses the
// search controller.
- (void)testSearchLongPressOnScrimCancelsSearchController {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Try long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      performAction:grey_longPress()];

  // Verify context menu is not visible.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:grey_nil()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Verifiy we went back to original folder content.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_notNil()];
}

// Tests cancelling search restores the node's bookmarks.
- (void)testSearchCancelRestoresNodeBookmarks {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search, tapping first to provide focus.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"X")];

  // Verify we have no items.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_nil()];

  // Cancel.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Verify all items are back.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_notNil()];
}

// Tests that the navigation bar isn't shown when search is focused and empty.
- (void)testSearchHidesNavigationBar {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Focus Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify we have no navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_nil()];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First")];

  // Verify we now have a navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can long press and edit a bookmark and see edits when going
// back to search.
- (void)testSearchLongPressEditOnURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First")];

  // Invoke Edit through context menu.
  [BookmarkEarlGreyUI
      tapOnLongPressContextMenuButton:chrome_test_util::
                                          BookmarksContextMenuEditButton()
                               onItem:TappableBookmarkNodeWithLabel(
                                          @"First URL")
                           openEditor:kBookmarkEditViewContainerIdentifier
                      modifyTextField:@"Title Field_textField"
                                   to:@"n6"
                          dismissWith:
                              kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Should not find it anymore.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_nil()];

  // Search with new name.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"n6")];

  // Should now find it again.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"n6")]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can long press and edit a bookmark folder and see edits
// when going back to search.
- (void)testSearchLongPressEditOnFolder {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  NSString* existingFolderTitle = @"Folder 1.1";

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(existingFolderTitle)];

  // Invoke Edit through long press.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          existingFolderTitle)]
      performAction:grey_longPress()];

  id<GREYMatcher> editFolderAction =
      chrome_test_util::BookmarksContextMenuEditButton();
  [[EarlGrey selectElementWithMatcher:editFolderAction]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  NSString* newFolderTitle = @"n7";
  [BookmarkEarlGreyUI renameBookmarkFolderWithFolderTitle:newFolderTitle];

  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify that the change has been made.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          existingFolderTitle)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(newFolderTitle)];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(newFolderTitle)]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can swipe URL items in search mode.
- (void)testSearchUrlCanBeSwipedToDelete {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First URL")];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:SwipeToShowDeleteButton()];

  // Verify we have a delete button.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can swipe folders in search mode.
- (void)testSearchFolderCanBeSwipedToDelete {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"Folder 1")];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:SwipeToShowDeleteButton()];

  // Verify we have a delete button.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can't search while in edit mode.
- (void)testDisablesSearchOnEditMode {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Verify search bar is enabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"UISearchBar")]
      assertWithMatcher:grey_userInteractionEnabled()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify search bar is disabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"UISearchBar")]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Cancel edito mode.
  [BookmarkEarlGreyUI closeContextBarEditMode];

  // Verify search bar is enabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"UISearchBar")]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests that new Folder is disabled when search results are shown.
- (void)testSearchDisablesNewFolderButtonOnNavigationBar {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Verify we now have a navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUI
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Tests that a single edit is possible when searching and selecting a single
// URL in edit mode.
- (void)testSearchEditModeEditOnSingleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  NSString* label = [NSString
      stringWithFormat:
          @"First URL, %@",
          base::SysUTF16ToNSString(
              url_formatter::
                  FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                      GetFirstUrl()))];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(label),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Invoke Edit through context menu.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"Title Field_textField"
                          to:@"n6"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Should not find it anymore.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_nil()];

  // Search with new name.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"n6")];

  // Should now find it again.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"n6")]
      assertWithMatcher:grey_notNil()];
}

// Tests that multiple deletes on search results works.
- (void)testSearchEditModeDeleteOnMultipleURL {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"URL")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  NSString* label = [NSString
      stringWithFormat:
          @"First URL, %@",
          base::SysUTF16ToNSString(
              url_formatter::
                  FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                      GetFirstUrl()))];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
  label = [NSString
      stringWithFormat:
          @"Second URL, %@",
          base::SysUTF16ToNSString(
              url_formatter::
                  FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                      GetSecondUrl()))];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];

  // Delete.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  // Should not find them anymore.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      assertWithMatcher:grey_nil()];

  // Should find other two URLs.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Third URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      assertWithMatcher:grey_notNil()];
}

// Tests that multiple moves on search results works.
- (void)testMoveFunctionalityOnMultipleUrlSelection {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"URL")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

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
                                                     newFolderEnabled:NO];

  // Cancel search.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

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

// Tests that a search and single edit is possible when searching over root.
- (void)testSearchEditPossibleOnRoot {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"First")];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  NSString* label = [NSString
      stringWithFormat:
          @"First URL, %@",
          base::SysUTF16ToNSString(
              url_formatter::
                  FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                      GetFirstUrl()))];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(label),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Invoke Edit through context menu.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"Title Field_textField"
                          to:@"n6"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Should not find it anymore.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      assertWithMatcher:grey_nil()];

  // Search with new name.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"n6")];

  // Should now find it again.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"n6")]
      assertWithMatcher:grey_notNil()];

  // Cancel search.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Verify we have no navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarksHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that you can search folders.
- (void)testSearchFolders {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Go down Folder 1 / Folder 2 / Folder 3.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 3")]
      performAction:grey_tap()];

  // Search and go to folder 1.1.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"Folder 1.1")];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Go back and verify we are in MobileBooknarks. (i.e. not back to Folder 2)
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Search and go to Folder 2.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_replaceText(@"Folder 2")];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];

  // Go back and verify we are in Folder 1. (i.e. not back to Mobile Bookmarks)
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];
}

@end
