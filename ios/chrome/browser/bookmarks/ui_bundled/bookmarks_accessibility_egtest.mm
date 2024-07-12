// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::MoveButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

// Bookmark accessibility tests for Chrome.
@interface BookmarksAccessibilityTestCase : WebHttpServerChromeTestCase
@end

@implementation BookmarksAccessibilityTestCase

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

#pragma mark - BookmarksAccessibilityTestCase Tests

// Tests that all elements on the bookmarks landing page are accessible.
- (void)testAccessibilityOnBookmarksLandingPage {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on mobile bookmarks are accessible.
- (void)testAccessibilityOnMobileBookmarks {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks folder Edit page are accessible.
- (void)testAccessibilityOnBookmarksFolderEditPage {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  id<GREYMatcher> editFolderMatcher =
      chrome_test_util::BookmarksContextMenuEditButton();
  [[EarlGrey selectElementWithMatcher:editFolderMatcher]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Edit page are accessible.
- (void)testAccessibilityOnBookmarksEditPage {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksContextMenuEditButton()]
      performAction:grey_tap()];

  // Wait until screen appears.
  id<GREYMatcher> screenTitleMatcher =
      grey_accessibilityID(kBookmarkEditViewContainerIdentifier);
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:screenTitleMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for Edit screen to appear.");

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Move page are accessible.
- (void)testAccessibilityOnBookmarksMovePage {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:MoveButton()] performAction:grey_tap()];

  // Wait until screen appears.
  id<GREYMatcher> screenTitleMatcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON));
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:screenTitleMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for Choose Folder screen to appear.");

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Move to New Folder page are
// accessible.
- (void)testAccessibilityOnBookmarksMoveToNewFolderPage {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:MoveButton()] performAction:grey_tap()];

  // Tap on "Create New Folder."
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on bookmarks Delete and Undo are accessible.
- (void)testAccessibilityOnBookmarksDeleteUndo {
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

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Select page are accessible.
- (void)testAccessibilityOnBookmarksSelect {
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarksHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

@end
