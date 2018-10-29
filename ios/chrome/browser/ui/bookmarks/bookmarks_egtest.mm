// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#include <vector>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/features.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksMenuButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CancelButton;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

namespace {

// GURL for the testing bookmark "First URL".
const GURL getFirstURL() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
}

// GURL for the testing bookmark "Second URL".
const GURL getSecondURL() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
}

// GURL for the testing bookmark "French URL".
const GURL getFrenchURL() {
  return web::test::HttpServer::MakeUrl("http://www.a.fr/");
}

// Matcher for bookmarks tool tip star. (used in iPad)
id<GREYMatcher> StarButton() {
  return ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
}

// Matcher for the button to edit bookmark.
id<GREYMatcher> EditBookmarkButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_EDIT);
}

// Matcher for the Delete button on the bookmarks UI.
id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

// Matcher for the Back button to |previousViewControllerLabel| on the bookmarks
// UI.
id<GREYMatcher> NavigateBackButtonTo(NSString* previousViewControllerLabel) {
  // When using the stock UINavigationBar back button item, the button's label
  // may be truncated to the word "Back", or to nothing at all.  It is not
  // possible to know which label will be used, as the OS makes that decision,
  // so try to search for any of them.
  id<GREYMatcher> buttonLabelMatcher =
      grey_anyOf(grey_accessibilityLabel(previousViewControllerLabel),
                 grey_accessibilityLabel(@"Back"), nil);

  if (@available(iOS 11, *)) {
    return grey_allOf(grey_kindOfClass([UIButton class]),
                      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                      buttonLabelMatcher, nil);
  } else {
    return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                      buttonLabelMatcher, nil);
  }
}

// Matcher for the DONE button on the bookmarks UI.
id<GREYMatcher> BookmarkHomeDoneButton() {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

// Matcher for the DONE button on the bookmarks edit UI.
id<GREYMatcher> BookmarksSaveEditDoneButton() {
  return grey_accessibilityID(kBookmarkEditNavigationBarDoneButtonIdentifier);
}

// Matcher for the DONE button on the bookmarks edit folder UI.
id<GREYMatcher> BookmarksSaveEditFolderButton() {
  return grey_accessibilityID(
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier);
}

// Matcher for context bar leading button.
id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeLeadingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for context bar center button.
id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeCenterButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for context bar trailing button.
id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeTrailingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for tappable bookmark node.
id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(label), grey_sufficientlyVisible(),
                    nil);
}

// Matcher for the search button.
id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kBookmarkHomeSearchBarIdentifier);
}

}  // namespace

// Bookmark integration tests for Chrome.
@interface BookmarksTestCase : ChromeTestCase
@end

@implementation BookmarksTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestCase Tests

// Verifies that adding a bookmark and removing a bookmark via the UI properly
// updates the BookmarkModel.
- (void)testAddRemoveBookmark {
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  // Add the bookmark from the UI.
  [BookmarksTestCase bookmarkCurrentTabWithTitle:bookmarkTitle];

  // Verify the bookmark is set.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkTitle expectedCount:1];

  // Verify the star is lit.
  if (!IsCompactWidth()) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(
                                     l10n_util::GetNSString(IDS_TOOLTIP_STAR))]
        assertWithMatcher:grey_notNil()];
  }

  // Open the BookmarkEditor.
  if (IsCompactWidth()) {
    [ChromeEarlGreyUI openToolsMenu];
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kToolsMenuEditBookmark),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(
                                     l10n_util::GetNSString(IDS_TOOLTIP_STAR))]
        performAction:grey_tap()];
  }

  // Delete the Bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Verify the bookmark is not in the BookmarkModel.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkTitle expectedCount:0];

  // Verify the the page is no longer bookmarked.
  if (IsCompactWidth()) {
    [ChromeEarlGreyUI openToolsMenu];
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kToolsMenuAddToBookmarks),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        assertWithMatcher:grey_notNil()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(
                                     l10n_util::GetNSString(IDS_TOOLTIP_STAR))]
        assertWithMatcher:grey_notNil()];
  }
  // Close the opened tab.
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() closeCurrentTab];
}

// Test to set bookmarks in multiple tabs.
- (void)testBookmarkMultipleTabs {
  const GURL firstURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  const GURL secondURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:firstURL];
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:secondURL];

  [BookmarksTestCase bookmarkCurrentTabWithTitle:@"my bookmark"];
  [BookmarksTestCase assertBookmarksWithTitle:@"my bookmark" expectedCount:1];
}

// Tests that changes to the parent folder from the Single Bookmark Editor
// are saved to the bookmark only when saving the results.
- (void)testMoveDoesSaveOnSave {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder.
  [BookmarksTestCase addFolderWithName:nil];

  // Verify that the editor is present.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Check that the new folder doesn't contain the bookmark.
  [BookmarksTestCase assertChildCount:0 ofFolderWithName:@"New Folder"];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Check that the new folder contains the bookmark.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"New Folder"];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Check that the new folder still contains the bookmark.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"New Folder"];
}

// Tests that keyboard commands are registered when a bookmark is added as it
// shows only a snackbar.
- (void)testKeyboardCommandsRegistered_AddBookmark {
  // Add the bookmark.
  [BookmarksTestCase starCurrentTab];
  GREYAssertTrue(chrome_test_util::GetRegisteredKeyCommandsCount() > 0,
                 @"Some keyboard commands are registered.");
}

// Tests that keyboard commands are not registered when a bookmark is edited, as
// the edit screen is presented modally.
- (void)testKeyboardCommandsNotRegistered_EditBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Edit the bookmark.
  if (!IsCompactWidth()) {
    [[EarlGrey selectElementWithMatcher:StarButton()] performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kToolsMenuEditBookmark),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:grey_accessibilityID(
                                 kPopupMenuToolsMenuTableViewId)]
        performAction:grey_tap()];
  }
  GREYAssertTrue(chrome_test_util::GetRegisteredKeyCommandsCount() == 0,
                 @"No keyboard commands are registered.");
}

// Test that swiping left to right navigate back.
- (void)testNavigateBackWithGesture {
  // Disabled on iPad as there is not "navigate back" gesture.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPad");
  }

// TODO(crbug.com/768339): This test is faling on devices because
// grey_swipeFastInDirectionWithStartPoint does not work.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on devices.");
#endif

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Make sure Mobile Bookmarks is not present. Also check the button Class to
  // avoid matching the "back" NavigationBar button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Mobile Bookmarks"),
                                   grey_kindOfClass([UITableViewCell class]),
                                   nil)] assertWithMatcher:grey_nil()];

  // Open the first folder, to be able to go back twice on the bookmarks.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Back twice using swipe left gesture.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionRight,
                                                            0.01, 0.5)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionRight,
                                                            0.01, 0.5)];

  // Check we navigated back to the Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mobile Bookmarks")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the bookmark context bar is shown in MobileBookmarks.
- (void)testBookmarkContextBarShown {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify the context bar's leading and trailing buttons are shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeLeadingButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
}

- (void)testBookmarkContextBarInVariousSelectionModes {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

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
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect Folder 1, so that Second URL is selected.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
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
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Cancel edit mode
  [BookmarksTestCase closeContextBarEditMode];

  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

// Tests when total height of bookmarks exceeds screen height.
- (void)testBookmarksExceedsScreenHeight {
  [BookmarksTestCase setupBookmarksWhichExceedsScreenHeight];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

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
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:YES];
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];
}

// TODO(crbug.com/801453): Folder name is not commited as expected in this test.
// Tests the new folder name is committed when "hide keyboard" button is
// pressed. (iPad specific)
- (void)DISABLED_testNewFolderNameCommittedWhenKeyboardDismissedOnIpad {
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not supported on iPhone");
  }
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
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
  [BookmarksTestCase verifyEmptyBackgroundAppears];
}

- (void)testEmptyBackgroundAndSelectButton {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Enter Folder 1.1 (which is empty)
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Verify the empty background appears.
  [BookmarksTestCase verifyEmptyBackgroundAppears];

  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify edit mode is close automatically (context bar switched back to
  // default state) and select button is disabled.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                    newFolderEnabled:YES];

  // Verify the empty background appears.
  [BookmarksTestCase verifyEmptyBackgroundAppears];
}

- (void)testCachePositionIsRecreated {
  [BookmarksTestCase setupBookmarksWhichExceedsScreenHeight];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Verify Bottom 1 is not visible before scrolling to bottom (make sure
  // setupBookmarksWhichExceedsScreenHeight works as expected).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom 1")]
      assertWithMatcher:grey_notVisible()];

  // Scroll to the bottom so that Bottom 1 is visible.
  [BookmarksTestCase scrollToBottom];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom 1")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Ensure the Bottom 1 of Folder 1 is visible.  That means both folder and
  // scroll position are restored successfully.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"Bottom 1, 127.0.0.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify root node is opened when cache position is deleted.
- (void)testCachePositionIsResetWhenNodeIsDeleted {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Select Folder 2.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Close bookmarks, it will store Folder 2 as the cache position.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Delete Folder 2.
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 2"];

  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Ensure the root node is opened, by verifying Mobile Bookmarks is seen in a
  // table cell.
  [BookmarksTestCase verifyBookmarkFolderIsSeen:@"Mobile Bookmarks"];
}

// Verify root node is opened when cache position is a permanent node and is
// empty.
- (void)testCachePositionIsResetWhenNodeIsPermanentAndEmpty {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Close bookmarks, it will store Mobile Bookmarks as the cache position.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Delete all bookmarks and folders under Mobile Bookmarks.
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 1.1"];
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 1"];
  [BookmarksTestCase removeBookmarkWithTitle:@"French URL"];
  [BookmarksTestCase removeBookmarkWithTitle:@"Second URL"];
  [BookmarksTestCase removeBookmarkWithTitle:@"First URL"];

  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Ensure the root node is opened, by verifying Mobile Bookmarks is seen in a
  // table cell.
  [BookmarksTestCase verifyBookmarkFolderIsSeen:@"Mobile Bookmarks"];
}

- (void)testCachePositionIsRecreatedWhenNodeIsMoved {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

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
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Move Folder 3 under Folder 1.
  [BookmarksTestCase moveBookmarkWithTitle:@"Folder 3"
                         toFolderWithTitle:@"Folder 1"];

  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Go back 1 level to Folder 1.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Folder 1")]
      performAction:grey_tap()];

  // Ensure we are at Folder 1, by verifying folders at this level.
  [BookmarksTestCase verifyBookmarkFolderIsSeen:@"Folder 2"];
}

// Tests that chrome://bookmarks is disabled.
- (void)testBookmarksURLDisabled {
  const std::string kChromeBookmarksURL = "chrome://bookmarks";
  [ChromeEarlGrey loadURL:GURL(kChromeBookmarksURL)];

  // Verify chrome://bookmarks appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(kChromeBookmarksURL)]
      assertWithMatcher:grey_notNil()];

  // Verify that the resulting page is an error page.
  std::string error = net::ErrorToShortString(net::ERR_INVALID_URL);
  [ChromeEarlGrey waitForWebViewContainingText:error];
}

#pragma mark - Helpers

// Navigates to the bookmark manager UI.
+ (void)openBookmarks {
  // Opens the bookmark manager.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:BookmarksMenuButton()];

  // Assert the menu is gone.
  [[EarlGrey selectElementWithMatcher:BookmarksMenuButton()]
      assertWithMatcher:grey_nil()];
}

// Selects |bookmarkFolder| to open.
+ (void)openBookmarkFolder:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"UITableViewCell")),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)] performAction:grey_tap()];
}

// Loads a set of default bookmarks in the model for the tests to use.
+ (void)setupStandardBookmarks {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  NSString* firstTitle = @"First URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(firstTitle), getFirstURL());

  NSString* secondTitle = @"Second URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(secondTitle), getSecondURL());

  NSString* frenchTitle = @"French URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(frenchTitle), getFrenchURL());

  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      bookmark_model->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  folderTitle = @"Folder 1.1";
  bookmark_model->AddFolder(bookmark_model->mobile_node(), 0,
                            base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 2";
  const bookmarks::BookmarkNode* folder2 = bookmark_model->AddFolder(
      folder1, 0, base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 3";
  const bookmarks::BookmarkNode* folder3 = bookmark_model->AddFolder(
      folder2, 0, base::SysNSStringToUTF16(folderTitle));

  const GURL thirdURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");
  NSString* thirdTitle = @"Third URL";
  bookmark_model->AddURL(folder3, 0, base::SysNSStringToUTF16(thirdTitle),
                         thirdURL);
}

// Loads a large set of bookmarks in the model which is longer than the screen
// height.
+ (void)setupBookmarksWhichExceedsScreenHeight {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  const GURL dummyURL = web::test::HttpServer::MakeUrl("http://google.com");
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(@"Bottom URL"), dummyURL);

  NSString* dummyTitle = @"Dummy URL";
  for (int i = 0; i < 20; i++) {
    bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                           base::SysNSStringToUTF16(dummyTitle), dummyURL);
  }
  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      bookmark_model->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(@"Top URL"), dummyURL);

  // Add URLs to Folder 1.
  bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                         dummyURL);
  bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(@"Bottom 1"),
                         dummyURL);
  for (int i = 0; i < 20; i++) {
    bookmark_model->AddURL(folder1, 0, base::SysNSStringToUTF16(dummyTitle),
                           dummyURL);
  }
}

// Selects MobileBookmarks to open.
+ (void)openMobileBookmarks {
  [BookmarksTestCase openBookmarkFolder:@"Mobile Bookmarks"];
}

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Verify the correct number of bookmarks exist.
  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  GREYAssertEqual(expectedCount, count, @"Unexpected number of bookmarks");
}

// Tap on the star to bookmark a page, then edit the bookmark to change the
// title to |title|.
+ (void)bookmarkCurrentTabWithTitle:(NSString*)title {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];
  // Add the bookmark from the UI.
  [BookmarksTestCase starCurrentTab];

  // Set the bookmark name.
  [[EarlGrey selectElementWithMatcher:EditBookmarkButton()]
      performAction:grey_tap()];
  NSString* titleIdentifier = @"Title Field_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(title)];

  // Dismiss the window.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
}

// Adds a bookmark for the current tab. Must be called when on a tab.
+ (void)starCurrentTab {
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuAddToBookmarks),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];
}

// Check that the currently edited bookmark is in |folderName| folder.
+ (void)assertFolderName:(NSString*)folderName {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(folderName), nil)]
      assertWithMatcher:grey_notNil()];
}

// Creates a new folder starting from the folder picker.
// Passing a |name| of 0 length will use the default value.
+ (void)addFolderWithName:(NSString*)name {
  // Wait for folder picker to appear.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Verify the folder creator is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Change the name of the folder.
  if (name.length > 0) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(@"Title_textField")]
        performAction:grey_replaceText(name)];
  }

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

// Asserts that a folder called |title| exists.
+ (void)assertFolderExists:(NSString*)title {
  base::string16 folderTitle16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  BOOL folderExists = NO;

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url())
      continue;
    // This is a folder.
    if (bookmark->GetTitle() == folderTitle16) {
      folderExists = YES;
      break;
    }
  }

  NSString* assertMessage =
      [NSString stringWithFormat:@"Folder %@ doesn't exist", title];
  GREYAssert(folderExists, assertMessage);
}

// Checks that the promo has already been seen or not.
+ (void)verifyPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return;
  }
  NSString* errorDesc = (seen)
                            ? @"Expected promo already seen, but it wasn't."
                            : @"Expected promo not already seen, but it was.";
  GREYFail(errorDesc);
}

// Checks that the promo has already been seen or not.
+ (void)setPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

// Waits for the disparition of the given |title| in the UI.
+ (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(title)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");
}

// Wait for Undo toast to go away.
+ (void)waitForUndoToastToGoAway {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for undo toast to go away");
}

// Waits for the bookmark model to be loaded in memory.
+ (void)waitForBookmarkModelLoaded:(BOOL)loaded {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout,
                 ^{
                   return bookmarkModel->loaded() == loaded;
                 }),
             @"Bookmark model was not loaded");
}

+ (void)assertExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(bookmark->GetTitle() == base::SysNSStringToUTF16(name),
             @"Could not find bookmark named %@ for %@", name, URL);
}

+ (void)assertAbsenceOfBookmarkWithURL:(NSString*)URL {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(!bookmark, @"There is a bookmark for %@", URL);
}

// Rename folder title to |folderTitle|. Must be in edit folder UI.
+ (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(folderTitle)];
}

// Dismisses the edit folder UI.
+ (void)closeEditBookmarkFolder {
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

// Close edit mode.
+ (void)closeContextBarEditMode {
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarCancelString])]
      performAction:grey_tap()];
}

// Select urls from Mobile Bookmarks and tap on a specified context bar button.
+ (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId {
  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Open All.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(buttonLabelId)]
      performAction:grey_tap()];
}

// Verify the Mobile Bookmarks's urls are open in the same order as they are in
// folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex {
  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "Second URL" appears.
  // TODO(crbug.com/695749): see we if can add switchToNextTab to
  // chrome_test_util so that we don't need to pass tabIndex here.
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex + 1);
  [[EarlGrey selectElementWithMatcher:OmniboxText(getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "First URL" appears.
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex + 2);
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Verifies that there is |count| children on the bookmark folder with |name|.
+ (void)assertChildCount:(int)count ofFolderWithName:(NSString*)name {
  base::string16 name16(base::SysNSStringToUTF16(name));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());

  const bookmarks::BookmarkNode* folder = NULL;
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_folder() && bookmark->GetTitle() == name16) {
      folder = bookmark;
      break;
    }
  }
  GREYAssert(folder, @"No folder named %@", name);
  GREYAssertEqual(
      folder->child_count(), count,
      @"Unexpected number of children in folder '%@': %d instead of %d", name,
      folder->child_count(), count);
}

+ (void)verifyContextMenuForSingleURL {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:grey_sufficientlyVisible()];

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

+ (void)verifyContextMenuForMultiAndMixedSelection {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

+ (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify context bar shows enabled "New Folder" and enabled "Select".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksTestCase
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   newFolderEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarSelectString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   selectEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
}

+ (void)verifyContextBarInEditMode {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      assertWithMatcher:grey_notNil()];
}

+ (void)verifyFolderFlowIsClosed {
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

+ (void)verifyEmptyBackgroundAppears {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkEmptyStateExplanatoryLabelIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Removes programmatically the first bookmark with the given title.
+ (void)removeBookmarkWithTitle:(NSString*)title {
  base::string16 name16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->GetTitle() == name16) {
      bookmarkModel->Remove(bookmark);
      return;
    }
  }
  GREYFail(@"Could not remove bookmark with name %@", title);
}

+ (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder {
  base::string16 name16(base::SysNSStringToUTF16(bookmarkTitle));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* bookmark = iterator.Next();
  while (iterator.has_next()) {
    if (bookmark->GetTitle() == name16) {
      break;
    }
    bookmark = iterator.Next();
  }

  base::string16 folderName16(base::SysNSStringToUTF16(newFolder));
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iteratorFolder(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* folder = iteratorFolder.Next();
  while (iteratorFolder.has_next()) {
    if (folder->GetTitle() == folderName16) {
      break;
    }
    folder = iteratorFolder.Next();
  }
  std::set<const bookmarks::BookmarkNode*> toMove;
  toMove.insert(bookmark);
  bookmark_utils_ios::MoveBookmarks(toMove, bookmarkModel, folder);
}

+ (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"UITableViewCell")),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Scroll the bookmarks to top.
+ (void)scrollToTop {
  // Provide a start points since it prevents some tests timing out under
  // certain configurations.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.5, 0.5)];
}

// Scroll the bookmarks to bottom.
+ (void)scrollToBottom {
  // Provide a start points since it prevents some tests timing out under
  // certain configurations.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeBottom, 0.5, 0.5)];
}

// Verify a folder with given name is created and it is not being edited.
+ (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle {
  // scroll to bottom to make sure new folder appears.
  [BookmarksTestCase scrollToBottom];
  // verify the folder is created.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      assertWithMatcher:grey_notNil()];
  // verify the editable textfield is gone.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_editing_text")]
      assertWithMatcher:grey_notVisible()];
}

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder {
  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the edit page (editor) is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Verify current parent folder for is correct.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(sourceFolder), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Folder to open folder picker.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

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
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(destinationFolder),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss edit page (editor).
  id<GREYMatcher> dismissMatcher = BookmarksSaveEditDoneButton();
  // If a folder is being edited use the EditFolder button dismiss matcher
  // instead.
  if ([editorId isEqualToString:kBookmarkFolderEditViewContainerIdentifier])
    dismissMatcher = BookmarksSaveEditFolderButton();
  [[EarlGrey selectElementWithMatcher:dismissMatcher] performAction:grey_tap()];

  // Verify the Editor was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];
}

+ (void)tapOnLongPressContextMenuButton:(int)menuButtonId
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through item context menu.
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(textFieldId)]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

+ (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(textFieldId)]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

// Context bar strings.
+ (NSString*)contextBarNewFolderString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
}

+ (NSString*)contextBarDeleteString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
}

+ (NSString*)contextBarCancelString {
  return l10n_util::GetNSString(IDS_CANCEL);
}

+ (NSString*)contextBarSelectString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
}

+ (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}

// Create a new folder with given title.
+ (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn {
  // Click on "New Folder".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeLeadingButtonIdentifier)]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"bookmark_editing_text";

  // Type the folder title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(titleIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_replaceText(folderTitle)];

  // Press the keyboard return key.
  if (pressReturn) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityID(titleIdentifier),
                                     grey_sufficientlyVisible(), nil)]
        performAction:grey_typeText(@"\n")];

    // Wait until the editing textfield is gone.
    [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:titleIdentifier];
  }
}

// TODO(crbug.com/695749): Add egtests for:
// 1. Spinner background.
// 2. Reorder bookmarks. (make sure it won't clear the row selection on table)
// 3. Test new folder name is committed when name editing is interrupted by
//    tapping context bar buttons.

@end

// Bookmark entries integration tests for Chrome.
@interface BookmarksTestCaseEntries : ChromeTestCase
@end

@implementation BookmarksTestCaseEntries

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestCaseEntries Tests

- (void)testUndoDeleteBookmarkFromSwipe {
  // TODO(crbug.com/851227): On Compact Width on iOS11, the
  // bookmark cell is being deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (@available(iOS 11, *)) {
    if (!IsCompactWidth()) {
      EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad on iOS11.");
    }
  }

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify context bar does not change when "Delete" shows up.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
  // Delete it.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify context bar remains in default state.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

// TODO(crbug.com/781445): Re-enable this test on 32-bit.
#if defined(ARCH_CPU_64_BITS)
#define MAYBE_testSwipeToDeleteDisabledInEditMode \
  testSwipeToDeleteDisabledInEditMode
#else
#define MAYBE_testSwipeToDeleteDisabledInEditMode \
  FLAKY_testSwipeToDeleteDisabledInEditMode
#endif
- (void)testSwipeToDeleteDisabledInEditMode {
  // TODO(crbug.com/851227): On non Compact Width on iOS11, the
  // bookmark cell is being deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (@available(iOS 11, *)) {
    if (!IsCompactWidth()) {
      EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad on iOS11.");
    }
  }

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify the delete confirmation button is gone after entering edit mode.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_nil()];

  // Swipe action on "Second URL".  This should not bring out delete
  // confirmation button as swipe-to-delete is disabled in edit mode.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button doesn't appear.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_nil()];

  // Cancel edit mode
  [BookmarksTestCase closeContextBarEditMode];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up. (swipe-to-delete is
  // re-enabled).
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForSingleURLSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarksTestCase verifyContextMenuForSingleURL];
}

// Verify Edit functionality on single URL selection.
- (void)testEditFunctionalityOnSingleURL {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // 1. Edit the bookmark title at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];

  // Modify the title.
  [BookmarksTestCase
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
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // 2. Edit the bookmark url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Modify the url.
  [BookmarksTestCase
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"URL Field_textField"
                          to:@"www.b.fr"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Verify that the bookmark was updated.
  [BookmarksTestCase assertExistenceOfBookmarkWithURL:@"http://www.b.fr/"
                                                 name:@"French URL"];

  // Press undo and verify the edit is undone.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];
  [BookmarksTestCase assertAbsenceOfBookmarkWithURL:@"http://www.b.fr/"];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // 3. Move a single url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single url.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Move the "Second URL" to "Folder 1.1".
  [BookmarksTestCase tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                                 openEditor:kBookmarkEditViewContainerIdentifier
                          setParentFolderTo:@"Folder 1.1"
                                       from:@"Mobile Bookmarks"];

  // Verify edit mode is stayed.
  [BookmarksTestCase verifyContextBarInEditMode];

  // Close edit mode.
  [BookmarksTestCase closeContextBarEditMode];

  // Navigate to "Folder 1.1" and verify "Second URL" is under it.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // 4. Test the cancel button at edit page.

  // Come back to the Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap cancel after modifying the url.
  [BookmarksTestCase tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                                 openEditor:kBookmarkEditViewContainerIdentifier
                            modifyTextField:@"URL Field_textField"
                                         to:@"www.b.fr"
                                dismissWith:@"Cancel"];

  // Verify that the bookmark was not updated.
  [BookmarksTestCase assertAbsenceOfBookmarkWithURL:@"http://www.b.fr/"];

  // Verify edit mode is stayed.
  [BookmarksTestCase verifyContextBarInEditMode];
}

// Verify Copy URL functionality on single URL selection.
- (void)testCopyFunctionalityOnSingleURL {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Invoke Copy through context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  // Select Copy URL.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      performAction:grey_tap()];

  // Verify general pasteboard has the URL copied.
  ConditionBlock condition = ^{
    return !![[UIPasteboard generalPasteboard].string
        containsString:@"www.a.fr"];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for URL to be copied to pasteboard.");

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

- (void)testContextMenuForMultipleURLSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
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
// TODO(crbug.com/816699): Re-enable this test on simulators.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuForMultipleURLOpenAll \
  testContextMenuForMultipleURLOpenAll
#else
#define MAYBE_testContextMenuForMultipleURLOpenAll \
  FLAKY_testContextMenuForMultipleURLOpenAll
#endif
- (void)MAYBE_testContextMenuForMultipleURLOpenAll {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Open 3 normal tabs from a normal session.
  [BookmarksTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                         IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 3 normal tabs.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 0,
                 @"Incognito tab count should be 0");

  // Verify the order of open tabs.
  [BookmarksTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  // Switch to Incognito mode by adding a new incognito tab.
  chrome_test_util::OpenNewIncognitoTab();

  [BookmarksTestCase openBookmarks];

  // Open 3 normal tabs from a incognito session.
  [BookmarksTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                         IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 6 normal tabs and no new incognito tabs.
  [ChromeEarlGrey waitForMainTabCount:6];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 1,
                 @"Incognito tab count should be 1");

  // Close the incognito tab to go back to normal mode.
  GREYAssert(chrome_test_util::CloseAllIncognitoTabs(), @"Tabs did not close");

  // The following verifies the selected bookmarks are open in the same order as
  // in folder.

  // Verify the order of open tabs.
  [BookmarksTestCase verifyOrderOfTabsWithCurrentTabIndex:3];
}

// Verify the Open All in Incognito functionality on multiple url selection.
// TODO(crbug.com/816699): Re-enable this test on simulators.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuForMultipleURLOpenAllInIncognito \
  testContextMenuForMultipleURLOpenAllInIncognito
#else
#define MAYBE_testContextMenuForMultipleURLOpenAllInIncognito \
  FLAKY_testContextMenuForMultipleURLOpenAllInIncognito
#endif
- (void)MAYBE_testContextMenuForMultipleURLOpenAllInIncognito {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Open 3 incognito tabs from a normal session.
  [BookmarksTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                         IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // Verify there are 3 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:3];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 1,
                 @"Main tab count should be 1");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");

  // Verify the order of open tabs.
  [BookmarksTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  [BookmarksTestCase openBookmarks];

  // Open 3 incognito tabs from a incognito session.
  [BookmarksTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                         IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // The 3rd tab will be re-used to open one of the selected bookmarks.  So
  // there will be 2 new tabs only.

  // Verify there are 5 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:5];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 1,
                 @"Main tab count should be 1");

  // Verify the order of open tabs.
  [BookmarksTestCase verifyOrderOfTabsWithCurrentTabIndex:2];
}

// Verify the Open and Open in Incognito functionality on single url.
- (void)testOpenSingleBookmarkInNormalAndIncognitoTab {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Open a bookmark in current tab in a normal session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase openBookmarks];

  // Open a bookmark in new tab from a normal session (through a long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify there is 1 new normal tab created and no new incognito tab created.
  [ChromeEarlGrey waitForMainTabCount:2];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 0,
                 @"Incognito tab count should be 0");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase openBookmarks];

  // Open a bookmark in an incognito tab from a normal session (through a long
  // press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify there is 1 incognito tab created and no new normal tab created.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase openBookmarks];

  // Open a bookmark in current tab from a incognito session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to staying at incognito mode");

  // Verify no new tabs created.
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 1,
                 @"Incognito tab count should be 1");
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  [BookmarksTestCase openBookmarks];

  // Open a bookmark in new incognito tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify a new incognito tab is created.
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to staying at incognito mode");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase openBookmarks];

  // Open a bookmark in a new normal tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify a new normal tab is created and no incognito tab is created.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 2,
                 @"Incognito tab count should be 2");

  // Verify the current tab is a normal tab.
  GREYAssertFalse(chrome_test_util::IsIncognitoMode(),
                  @"Failed to switch to normal mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForMixedSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarksTestCase verifyContextMenuForMultiAndMixedSelection];
}

- (void)testLongPressOnSingleURL {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  // Verify context menu.
  [BookmarksTestCase verifyContextMenuForSingleURL];
}

// Verify Move functionality on mixed folder / url selection.
- (void)testMoveFunctionalityOnMixedSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
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
  [BookmarksTestCase removeBookmarkWithTitle:@"First URL"];

  // Choose to move into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarksTestCase verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // Verify new folder "Title For New Folder" has two bookmark nodes.
  [BookmarksTestCase assertChildCount:2
                     ofFolderWithName:@"Title For New Folder"];

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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
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
  [BookmarksTestCase verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // Verify Folder 1 has three bookmark nodes.
  [BookmarksTestCase assertChildCount:3 ofFolderWithName:@"Folder 1"];

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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
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
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 1"];
  [BookmarksTestCase removeBookmarkWithTitle:@"Second URL"];

  // Verify folder picker is exited.
  [BookmarksTestCase verifyFolderFlowIsClosed];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
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
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
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
                                   [BookmarksTestCase contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)testDeleteSingleURLNode {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

- (void)testDeleteMultipleNodes {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

@end

// Bookmark promo integration tests for Chrome.
@interface BookmarksTestCasePromo : ChromeTestCase
@end

@implementation BookmarksTestCasePromo

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestCasePromo Tests

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarksTestCase setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Go to child node.
  [BookmarksTestCase openMobileBookmarks];

  // Wait until promo is gone.
  [SigninEarlGreyUI checkSigninPromoNotVisible];

  // Check that the promo already seen state is not updated.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];

  // Come back to root node, and the promo view should appear.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Bookmarks")]
      performAction:grey_tap()];

  // Check promo view is still visible.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping No thanks on the promo make it disappear.
- (void)testPromoNoThanksMakeItDisappear {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarksTestCase setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSigninPromoCloseButtonId)]
      performAction:grey_tap()];

  // Wait until promo is gone.
  [SigninEarlGreyUI checkSigninPromoNotVisible];

  // Check that the promo already seen state is updated.
  [BookmarksTestCase verifyPromoAlreadySeen:YES];
}

// Tests the tapping on the primary button of sign-in promo view in a cold
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithColdStateUsingPrimaryButton {
  [BookmarksTestCase openBookmarks];

  // Check that sign-in promo view are visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_buttonTitle(@"Cancel"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
}

// Tests the tapping on the primary button of sign-in promo view in a warm
// state makes the confirmaiton sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingPrimaryButton {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that promo is visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the Sign in button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoSecondaryButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:
                 grey_buttonTitle([l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)
                     uppercaseString])] performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  [BookmarksTestCase verifyPromoAlreadySeen:NO];
}

// Tests the tapping on the secondary button of sign-in promo view in a warm
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that sign-in promo view are visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the secondary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:
                 grey_buttonTitle([l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)
                     uppercaseString])] performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
}

// Tests that the sign-in promo should not be shown after been shown 19 times.
- (void)testAutomaticSigninPromoDismiss {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, 19);
  [BookmarksTestCase openBookmarks];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Check the sign-in promo already-seen state didn't change.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  GREYAssertEqual(
      20, prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount),
      @"Should have incremented the display count");
  // Close the bookmark view and open it again.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];
  [BookmarksTestCase openBookmarks];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI checkSigninPromoNotVisible];
}

@end

// Bookmark accessibility tests for Chrome.
@interface BookmarksTestCaseAccessibility : ChromeTestCase
@end

@implementation BookmarksTestCaseAccessibility

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestCaseAccessibility Tests

// Tests that all elements on the bookmarks landing page are accessible.
- (void)testAccessibilityOnBookmarksLandingPage {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on mobile bookmarks are accessible.
- (void)testAccessibilityOnMobileBookmarks {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks folder Edit page are accessible.
- (void)testAccessibilityOnBookmarksFolderEditPage {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Edit page are accessible.
- (void)testAccessibilityOnBookmarksEditPage {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move page are accessible.
- (void)testAccessibilityOnBookmarksMovePage {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move to New Folder page are
// accessible.
- (void)testAccessibilityOnBookmarksMoveToNewFolderPage {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on bookmarks Delete and Undo are accessible.
- (void)testAccessibilityOnBookmarksDeleteUndo {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Select page are accessible.
- (void)testAccessibilityOnBookmarksSelect {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

@end

// Bookmark folders integration tests for Chrome.
@interface BookmarksTestCaseFolders : ChromeTestCase
@end

@implementation BookmarksTestCaseFolders

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestFolders Tests

// Tests moving bookmarks into a new folder created in the moving process.
- (void)testCreateNewFolderWhileMovingBookmarks {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Move.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move the bookmark into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder (Change Folder) is Bookmarks folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder (Change Folder).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new Change Folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder (Change Folder) is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
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

  // Verify new folder has been created under Folder 2.
  [BookmarksTestCase assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder has two bookmarks.
  [BookmarksTestCase assertChildCount:2
                     ofFolderWithName:@"Title For New Folder"];
}

- (void)testCantDeleteFolderBeingEdited {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Swipe action to try to delete the newly created folder while its name its
  // being edited.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"New Folder")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button doesn't show up.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testNavigateAwayFromFolderBeingEdited {
  [BookmarksTestCase setupBookmarksWhichExceedsScreenHeight];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify bottom URL is not visible before scrolling to bottom (make sure
  // setupBookmarksWhichExceedsScreenHeight works as expected).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Bottom URL")]
      assertWithMatcher:grey_notVisible()];

  // Verify the top URL is visible (isn't covered by the navigation bar).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Top URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Test new folder could be created.  This verifies bookmarks scrolled to
  // bottom successfully for folder name editng.
  NSString* newFolderTitle = @"New Folder";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Scroll to top to navigate away from the folder being created.
  [BookmarksTestCase scrollToTop];

  // Scroll back to the Folder being created.
  [BookmarksTestCase scrollToBottom];

  // Folder should still be in Edit mode, because of this match for Value.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityValue(@"New Folder")]
      assertWithMatcher:grey_notNil()];
}

- (void)testDeleteSingleFolderNode {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

// Test when current navigating folder is deleted in background, empty
// background should be shown with context bar buttons disabled.
- (void)testWhenCurrentFolderDeletedInBackground {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Enter Folder 1
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Enter Folder 2
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Delete the Folder 1 and Folder 2 programmatically in background.
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 2"];
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 1"];

  // Verify edit mode is close automatically (context bar switched back to
  // default state) and both select and new folder button are disabled.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                    newFolderEnabled:NO];

  // Verify the empty background appears.
  [BookmarksTestCase verifyEmptyBackgroundAppears];

  // Come back to Folder 1 (which is also deleted).
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Folder 1")]
      performAction:grey_tap()];

  // Verify both select and new folder button are disabled.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:NO
                                                    newFolderEnabled:NO];

  // Verify the empty background appears.
  [BookmarksTestCase verifyEmptyBackgroundAppears];

  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Ensure Folder 1.1 is seen, that means it successfully comes back to Mobile
  // Bookmarks.
  [BookmarksTestCase verifyBookmarkFolderIsSeen:@"Folder 1.1"];
}

- (void)testLongPressOnSingleFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the context menu. On non compact width tap the Bookmarks TableView
  // to dismiss, since there might not be a cancel button.
  if (IsCompactWidth()) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
        performAction:grey_tap()];
  }

  // Come back to the root.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Bookmarks")]
      performAction:grey_tap()];

  // Long press on Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Mobile Bookmarks")]
      performAction:grey_longPress()];

  // Verify it doesn't show the context menu. (long press is disabled on
  // permanent node.)
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_nil()];
}

// Verify Edit functionality for single folder selection.
- (void)testEditFunctionalityOnSingleFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // 1. Edit the folder title at edit page.

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
  NSString* existingFolderTitle = @"Folder 1";
  NSString* newFolderTitle = @"New Folder Title";
  [BookmarksTestCase renameBookmarkFolderWithFolderTitle:newFolderTitle];

  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify that the change has been made.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(existingFolderTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // 2. Move a single folder at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(newFolderTitle)]
      performAction:grey_tap()];

  // Move the "New Folder Title" to "Folder 1.1".
  [BookmarksTestCase
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER
                  openEditor:kBookmarkFolderEditViewContainerIdentifier
           setParentFolderTo:@"Folder 1.1"
                        from:@"Mobile Bookmarks"];

  // Verify edit mode remains.
  [BookmarksTestCase verifyContextBarInEditMode];

  // Close edit mode.
  [BookmarksTestCase closeContextBarEditMode];

  // Navigate to "Folder 1.1" and verify "New Folder Title" is under it.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // 3. Test the cancel button at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      performAction:grey_tap()];

  // Tap cancel after modifying the title.
  [BookmarksTestCase
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER
                  openEditor:kBookmarkFolderEditViewContainerIdentifier
             modifyTextField:@"Title_textField"
                          to:@"Dummy"
                 dismissWith:@"Cancel"];

  // Verify that the bookmark was not updated.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify edit mode is stayed.
  [BookmarksTestCase verifyContextBarInEditMode];

  // 4. Test the delete button at edit page.

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditorDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify that the folder is deleted.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notVisible()];

  // 5. Verify that when adding a new folder, edit mode will not mistakenly come
  // back (crbug.com/781783).

  // Create a new folder.
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:YES];

  // Tap on the new folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      performAction:grey_tap()];

  // Verify we enter the new folder. (instead of selecting it in edit mode).
  [BookmarksTestCase verifyEmptyBackgroundAppears];
}

// Verify Move functionality on single folder through long press.
- (void)testMoveFunctionalityOnSingleFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move the bookmark folder - "Folder 1" into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder for "Title For New Folder" folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new parent folder for "Title For New Folder".
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarksTestCase verifyFolderFlowIsClosed];

  // Verify new folder "Title For New Folder" has been created under Folder 2.
  [BookmarksTestCase assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder "Title For New Folder" has one bookmark folder.
  [BookmarksTestCase assertChildCount:1
                     ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" has been moved and assert it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple folder selection.
- (void)testMoveFunctionalityOnMultipleFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select multiple folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into a new folder. By tapping on the New Folder Cell.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarksTestCase verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];

  // Verify new folder "Title For New Folder" has two bookmark folder.
  [BookmarksTestCase assertChildCount:2
                     ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextBarForSingleFolderSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify it shows edit view controller.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForMultipleFolderSelection {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarksTestCase contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarksTestCase verifyContextMenuForMultiAndMixedSelection];
}

// Tests that the default folder bookmarks are saved in is updated to the last
// used folder.
- (void)testStickyDefaultFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder.
  [BookmarksTestCase addFolderWithName:@"Sticky Folder"];

  // Verify that the editor is present.  Uses notNil() instead of
  // sufficientlyVisible() because the large title in the navigation bar causes
  // less than 75% of the table view to be visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];

  // Second, bookmark a page.

  // Verify that the bookmark that is going to be added is not in the
  // BookmarkModel.
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/fullscreen.html");
  NSString* const bookmarkedURLString =
      base::SysUTF8ToNSString(bookmarkedURL.spec());
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkedURLString
                                expectedCount:0];
  // Open the page.
  std::string expectedURLContent = bookmarkedURL.GetContent();
  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  // Verify that the folder has only one element.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Sticky Folder"];

  // Bookmark the page.
  [BookmarksTestCase starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"Bookmarked to Sticky Folder")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkedURLString
                                expectedCount:1];

  // Verify that the folder has now two elements.
  [BookmarksTestCase assertChildCount:2 ofFolderWithName:@"Sticky Folder"];
}

// Tests the new folder name is committed when name editing is interrupted by
// navigating away.
- (void)testNewFolderNameCommittedOnNavigatingAway {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Interrupt the folder name editing by tapping on back.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Bookmarks")]
      performAction:grey_tap()];

  // Come back to Mobile Bookmarks.
  [BookmarksTestCase openMobileBookmarks];

  // Verify folder name "New Folder 1" was committed.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 2" without pressing return.
  newFolderTitle = @"New Folder 2";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Interrupt the folder name editing by tapping on done.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Verify folder name "New Folder 2" was committed.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 3" without pressing return.
  newFolderTitle = @"New Folder 3";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Interrupt the folder name editing by entering Folder 1
  [BookmarksTestCase scrollToTop];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Verify folder name "New Folder 3" was committed.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 4" without pressing return.
  newFolderTitle = @"New Folder 4";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:NO];

  // Interrupt the folder name editing by tapping on First URL.
  [BookmarksTestCase scrollToTop];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarksTestCase openBookmarks];

  // Verify folder name "New Folder 4" was committed.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];
}

// Tests the creation of new folders by tapping on 'New Folder' button of the
// context bar.
- (void)testCreateNewFolderWithContextBar {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Create a new folder and name it "New Folder 1".
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:YES];

  // Verify "New Folder 1" is created.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and name it "New Folder 2".
  newFolderTitle = @"New Folder 2";
  [BookmarksTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                pressReturn:YES];

  // Verify "New Folder 2" is created.
  [BookmarksTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Verify context bar does not change after editing folder name.
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:YES];
}

// Test the creation of a bookmark and new folder (by tapping on the star).
- (void)testAddBookmarkInNewFolder {
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Bookmarked")]
      assertWithMatcher:grey_notNil()];

  // Tap on the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarksTestCase
      assertBookmarksWithTitle:base::SysUTF8ToNSString(expectedURLContent)
                 expectedCount:1];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase assertFolderName:@"Mobile Bookmarks"];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder with default name.
  [BookmarksTestCase addFolderWithName:nil];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase assertFolderExists:@"New Folder"];
}

@end

// Bookmark search integration tests for Chrome.
@interface BookmarksTestCaseSearch : ChromeTestCase
@end

@implementation BookmarksTestCaseSearch

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksTestCaseSearch Tests

// Tests that the search bar is shown on root.
- (void)testSearchBarShownOnRoot {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  // Verify the search bar is shown.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_userInteractionEnabled(), nil)];
}

// Tests that the search bar is shown on mobile list.
- (void)testSearchBarShownOnMobileBookmarks {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify the search bar is shown.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_userInteractionEnabled(), nil)];
}

// Tests the search.
- (void)testSearchResults {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

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

  // Search 'o'.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"o")];

  // Verify that folders are not filtered out.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Search 'on'.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"n")];

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
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"y")];

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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search 'zz'.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"zz\n")];

  // Verify that we have a 'No Results' label somewhere.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_HISTORY_NO_SEARCH_RESULTS))]
      assertWithMatcher:grey_notNil()];

  // Verify that Edit button is disabled.
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarksTestCase contextBarSelectString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Tests that scrim is shown while search box is enabled with no queries.
- (void)testSearchScrimShownWhenSearchBoxEnabled {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify that scrim is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Searching.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"i")];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Go back to original folder content.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_clearText()];

  // Verify that scrim is visible again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Cancel.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping scrim while search box is enabled dismisses the search
// controller.
- (void)testSearchTapOnScrimCancelsSearchController {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Tap on scrim.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
      performAction:grey_tap()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeSearchScrimIdentifier)]
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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"X")];

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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Focus Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Verify we have no navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_nil()];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First")];

  // Verify we now have a navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can long press and edit a bookmark and see edits when going
// back to search.
- (void)testSearchLongPressEditOnURL {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First")];

  // Invoke Edit through context menu.
  [BookmarksTestCase
      tapOnLongPressContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  NSString* existingFolderTitle = @"Folder 1.1";

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(existingFolderTitle)];

  // Invoke Edit through long press.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          existingFolderTitle)]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  NSString* newFolderTitle = @"n7";
  [BookmarksTestCase renameBookmarkFolderWithFolderTitle:newFolderTitle];

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
  // TODO(crbug.com/851227): On non Compact Width on iOS11, the
  // bookmark cell is being deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (@available(iOS 11, *)) {
    if (!IsCompactWidth()) {
      EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad on iOS11.");
    }
  }

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First URL")];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify we have a delete button.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can swipe folders in search mode.
- (void)testSearchFolderCanBeSwipedToDelete {
  // TODO(crbug.com/851227): On non Compact Width on iOS11, the
  // bookmark cell is being deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (@available(iOS 11, *)) {
    if (!IsCompactWidth()) {
      EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad on iOS11.");
    }
  }

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"Folder 1")];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify we have a delete button.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests that you can't search while in edit mode.
- (void)testDisablesSearchOnEditMode {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify search bar is enabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass(
                                          NSClassFromString(@"UISearchBar"))]
      assertWithMatcher:grey_userInteractionEnabled()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify search bar is disabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass(
                                          NSClassFromString(@"UISearchBar"))]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Cancel edito mode.
  [BookmarksTestCase closeContextBarEditMode];

  // Verify search bar is enabled.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass(
                                          NSClassFromString(@"UISearchBar"))]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests that new Folder is disabled when search results are shown.
- (void)testSearchDisablesNewFolderButtonOnNavigationBar {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First\n")];

  // Verify we now have a navigation bar.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksTestCase
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Tests that a single edit is possible when searching and selecting a single
// URL in edit mode.
- (void)testSearchEditModeEditOnSingleURL {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First\n")];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"First URL, 127.0.0.1")]
      performAction:grey_tap()];

  // Invoke Edit through context menu.
  [BookmarksTestCase
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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"URL\n")];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"First URL, 127.0.0.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"Second URL, 127.0.0.1")]
      performAction:grey_tap()];

  // Delete.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarksTestCase contextBarDeleteString])]
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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"URL\n")];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
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
                                   [BookmarksTestCase contextBarMoreString])]
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
  [BookmarksTestCase verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksTestCase waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarksTestCase verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                    newFolderEnabled:NO];

  // Cancel search.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  // Verify Folder 1 has three bookmark nodes.
  [BookmarksTestCase assertChildCount:3 ofFolderWithName:@"Folder 1"];

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
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];

  // Search and hide keyboard.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"First\n")];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"First URL, 127.0.0.1")]
      performAction:grey_tap()];

  // Invoke Edit through context menu.
  [BookmarksTestCase
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
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that you can search folders.
- (void)testSearchFolders {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarks];
  [BookmarksTestCase openMobileBookmarks];

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
      performAction:grey_typeText(@"Folder 1.1")];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Go back and verify we are in MobileBooknarks. (i.e. not back to Folder 2)
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Mobile Bookmarks")]
      performAction:grey_tap()];

  // Search and go to Folder 2.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(@"Folder 2")];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];

  // Go back and verify we are in Folder 1. (i.e. not back to Mobile Bookmarks)
  [[EarlGrey selectElementWithMatcher:NavigateBackButtonTo(@"Folder 1")]
      performAction:grey_tap()];
}

@end
