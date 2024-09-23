// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_HOME_NODE_ITEM_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_HOME_NODE_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

namespace bookmarks {
class BookmarkNode;
}

// BookmarksHomeNodeItem provides data for a table view row that displays a
// single bookmark.
@interface BookmarksHomeNodeItem : TableViewItem

// The BookmarkNode that backs this item.
@property(nonatomic, readwrite, assign)
    const bookmarks::BookmarkNode* bookmarkNode;

// Whether a slashed cloud should be displayed.
@property(nonatomic, assign) BOOL shouldDisplayCloudSlashIcon;

- (instancetype)initWithType:(NSInteger)type
                bookmarkNode:(const bookmarks::BookmarkNode*)node
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_HOME_NODE_ITEM_H_
