// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_NODE_ITEM_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_NODE_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@class FaviconAttributes;

// BookmarksHomeNodeItem provides data for a table view row that displays a
// single bookmark.
@interface BookmarksHomeNodeItem : TableViewItem

// Whether a slashed cloud should be displayed.
@property(nonatomic, assign) BOOL shouldDisplayCloudSlashIcon;

// Attributes for the favicon.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

// The node is not retained.
+ (instancetype)makeItemWithType:(NSInteger)type
                    bookmarkNode:(const bookmarks::BookmarkNode*)node;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// Returns the node associated to this item if it still exists, otherwise nil.
- (const bookmarks::BookmarkNode*)bookmarkNode:
    (const bookmarks::BookmarkModel*)model;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_NODE_ITEM_H_
