// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item to display the name of the parent folder of a bookmark node.
@interface BookmarkParentFolderItem : TableViewItem

// The title of the bookmark folder it represents.
@property(nonatomic, copy) NSString* title;

@end

// Cell class associated to BookmarkParentFolderItem.
@interface BookmarkParentFolderCell : TableViewCell

// Label that displays the item's title.
@property(nonatomic, readonly, strong) UILabel* parentFolderNameLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_PARENT_FOLDER_ITEM_H_
