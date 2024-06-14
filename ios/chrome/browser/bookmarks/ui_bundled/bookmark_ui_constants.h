// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UI_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Bookmark container constants:
// Accessibility identifier of the Bookmark Home View container.
extern NSString* const kBookmarksHomeViewContainerIdentifier;
// Accessibility identifier of the Bookmark Edit View container.
extern NSString* const kBookmarkEditViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Edit View container.
extern NSString* const kBookmarkFolderEditViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Create View container.
extern NSString* const kBookmarkFolderCreateViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Picker View container.
extern NSString* const kBookmarkFolderPickerViewContainerIdentifier;
// Accessibility identifier of the Bookmark Home TableView.
extern NSString* const kBookmarksHomeTableViewIdentifier;
// Accessibility identifier of the Bookmark Home context menu.
extern NSString* const kBookmarksHomeContextMenuIdentifier;

// UINavigationBar accessibility constants:
// Accessibility identifier of the Bookmark navigation bar.
extern NSString* const kBookmarkNavigationBarIdentifier;
// Accessibility identifier of the BookmarksHome VC navigation bar done button.
extern NSString* const kBookmarksHomeNavigationBarDoneButtonIdentifier;
// Accessibility identifier of the BookmarkEdit VC navigation bar done button.
extern NSString* const kBookmarkEditNavigationBarDoneButtonIdentifier;
// Accessibility identifier of the BookmarkFolderEdit VC navigation bar done
// button.
extern NSString* const kBookmarkFolderEditNavigationBarDoneButtonIdentifier;

// UIToolbar accessibility constants:
// Accessibility identifier of the BookmarkEditVC toolbar delete button.
extern NSString* const kBookmarkEditDeleteButtonIdentifier;
// Accessibility identifier of the BookmarkFolderEditorVC toolbar delete button.
extern NSString* const kBookmarkFolderEditorDeleteButtonIdentifier;
// Accessibility identifier of the BookmarksHomeVC leading button.
extern NSString* const kBookmarksHomeLeadingButtonIdentifier;
// Accessibility identifier of the BookmarksHomeVC center button.
extern NSString* const kBookmarksHomeCenterButtonIdentifier;
// Accessibility identifier of the BookmarksHomeVC trailing button.
extern NSString* const kBookmarksHomeTrailingButtonIdentifier;
// Accessibility identifier of the BookmarksHomeVC UIToolbar.
extern NSString* const kBookmarksHomeUIToolbarIdentifier;
// Accessibility identifier of the BookmarksHomeVC search bar.
extern NSString* const kBookmarksHomeSearchBarIdentifier;
// Accessibility identifier of the search scrim.
extern NSString* const kBookmarksHomeSearchScrimIdentifier;

// Cell Layout constants:
// The space between UIViews inside the cell.
extern const CGFloat kBookmarkCellViewSpacing;
// The vertical space between the Cell margin and its contents.
extern const CGFloat kBookmarkCellVerticalInset;
// The horizontal leading space between the Cell margin and its contents.
extern const CGFloat kBookmarkCellHorizontalLeadingInset;
// The horizontal trailing space between the Cell margin and its contents.
extern const CGFloat kBookmarkCellHorizontalTrailingInset;
// The horizontal space between the Cell content and its accessory view.
extern const CGFloat kBookmarkCellHorizontalAccessoryViewSpacing;

// Cell accessibility constants:
// Accessibility identifier of the Create NewFolder Button in profile bookmarks
// section.
extern NSString* const kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier;

// Cell accessibility constants:
// Accessibility identifier of the Create NewFolder Button in account bookmarks
// section.
extern NSString* const kBookmarkCreateNewAccountFolderCellIdentifier;

// Empty state accessibility constants:
// Accessibility identifier for the explanatory label in the empty state.
extern NSString* const kBookmarkEmptyStateExplanatoryLabelIdentifier;

// Accessibility identifiers for batch upload views.
extern NSString* const kBookmarksHomeBatchUploadRecommendationItemIdentifier;
extern NSString* const kBookmarksHomeBatchUploadButtonIdentifier;
extern NSString* const kBookmarksHomeBatchUploadAlertIdentifier;

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UI_CONSTANTS_H_
