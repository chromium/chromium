// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_TABLE_VIEW_BOOKMARKS_FOLDER_ITEM_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_TABLE_VIEW_BOOKMARKS_FOLDER_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_table_cell_title_editing.h"

typedef NS_ENUM(NSInteger, TableViewBookmarksFolderStyle) {
  BookmarksFolderStyleFolderEntry,
  BookmarksFolderStyleNewFolder,
};

typedef NS_ENUM(NSInteger, TableViewBookmarksFolderAccessoryType) {
  BookmarksFolderAccessoryTypeNone,
  BookmarksFolderAccessoryTypeCheckmark,
  BookmarksFolderAccessoryTypeDisclosureIndicator,
};

// TableViewBookmarksFolderItem provides data for a table view row that
// displays a single bookmark folder.
@interface TableViewBookmarksFolderItem : TableViewItem

// The Item's designated initializer. If `style` is
// TableViewBookmarksFolderStyle then all other property values will be
// ignored.
- (instancetype)initWithType:(NSInteger)type
                       style:(TableViewBookmarksFolderStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// The item's title.
@property(nonatomic, copy) NSString* title;
// YES if the FolderItem is the current folder.
@property(nonatomic, assign, getter=isCurrentFolder) BOOL currentFolder;
// The item's left indentation level.
@property(nonatomic, assign) NSInteger indentationLevel;
// Whether a slashed cloud should be displayed.
@property(nonatomic, assign) BOOL shouldDisplayCloudSlashIcon;

@end

// TableViewCell that displays BookmarkFolderItem data.
@interface TableViewBookmarksFolderCell
    : TableViewCell <BookmarkTableCellTitleEditing>

// The leading constraint used to set the cell's leading indentation. The
// default indentationLevel property doesn't affect any custom Cell subviews,
// changing `indentationConstraint` constant property will.
@property(nonatomic, strong, readonly)
    NSLayoutConstraint* indentationConstraint;
// The folder image displayed by this cell.
@property(nonatomic, strong) UIImageView* folderImageView;
// The folder title displayed by this cell.
@property(nonatomic, strong) UITextField* folderTitleTextField;
// Accessory Type.
@property(nonatomic, assign)
    TableViewBookmarksFolderAccessoryType bookmarksAccessoryType;
// A view containing a slashed cloud icon; at the end of the subview stack.
@property(nonatomic, strong) UIView* cloudSlashedView;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_TABLE_VIEW_BOOKMARKS_FOLDER_ITEM_H_
