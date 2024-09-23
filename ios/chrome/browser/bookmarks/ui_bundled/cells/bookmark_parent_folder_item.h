// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item to display the name of the parent folder of a bookmark node.
@interface BookmarkParentFolderItem : TableViewItem

// The title of the bookmark folder it represents.
@property(nonatomic, copy) NSString* title;

// Whether a slashed cloud should be displayed
@property(nonatomic, assign) BOOL shouldDisplayCloudSlashIcon;

@end

// Cell class associated to BookmarkParentFolderItem.
@interface BookmarkParentFolderCell : TableViewCell

// Label that displays the item's title.
@property(nonatomic, readonly, strong) UILabel* parentFolderNameLabel;

// A view containing a slashed cloud icon; at the end of the subview stack.
@property(nonatomic, strong) UIView* cloudSlashedView;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
