// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Bookmark container constants:
// Accessibility identifier of the Bookmark Home View container.
extern NSString* const kBookmarkHomeViewContainerIdentifier;
// Accessibility identifier of the Bookmark Edit View container.
extern NSString* const kBookmarkEditViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Edit View container.
extern NSString* const kBookmarkFolderEditViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Create View container.
extern NSString* const kBookmarkFolderCreateViewContainerIdentifier;
// Accessibility identifier of the Bookmark Folder Picker View container.
extern NSString* const kBookmarkFolderPickerViewContainerIdentifier;
// Accessibility identifier of the Bookmark Home TableView.
extern NSString* const kBookmarkHomeTableViewIdentifier;
// Accessibility identifier of the Bookmark Home context menu.
extern NSString* const kBookmarkHomeContextMenuIdentifier;

// UINavigationBar accessibility constants:
// Accessibility identifier of the Bookmark navigation bar.
extern NSString* const kBookmarkNavigationBarIdentifier;
// Accessibility identifier of the BookmarkHome VC navigation bar done button.
extern NSString* const kBookmarkHomeNavigationBarDoneButtonIdentifier;
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
// Accessibility identifier of the BookmarkHomeVC leading button.
extern NSString* const kBookmarkHomeLeadingButtonIdentifier;
// Accessibility identifier of the BookmarkHomeVC center button.
extern NSString* const kBookmarkHomeCenterButtonIdentifier;
// Accessibility identifier of the BookmarkHomeVC trailing button.
extern NSString* const kBookmarkHomeTrailingButtonIdentifier;
// Accessibility identifier of the BookmarkHomeVC UIToolbar.
extern NSString* const kBookmarkHomeUIToolbarIdentifier;
// Accessibility identifier of the BookmarkHomeVC search bar.
extern NSString* const kBookmarkHomeSearchBarIdentifier;
// Accessibility identifier of the search scrim.
extern NSString* const kBookmarkHomeSearchScrimIdentifier;

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
// Accessibility identifier of the Create NewFolder Button.
extern NSString* const kBookmarkCreateNewFolderCellIdentifier;

// Empty state accessibility constants:
// Accessibility identifier for the explanatory label in the empty state.
extern NSString* const kBookmarkEmptyStateExplanatoryLabelIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_CONSTANTS_H_
