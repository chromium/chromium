// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol BookmarkModelBridgeObserver <NSObject>
// The bookmark model has loaded.
- (void)bookmarkModelLoaded;
// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode;
// The node has not changed, but the ordering and existence of its children have
// changed.
- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode;
// The node has moved to a new parent folder.
- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent;
// `node` was deleted from `folder`.
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder;
// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes;

@optional
// The node favicon changed.
- (void)bookmarkNodeFaviconChanged:(const bookmarks::BookmarkNode*)bookmarkNode;
@end

// A bridge that translates BookmarkModelObserver C++ callbacks into ObjC
// callbacks.
class BookmarkModelBridge : public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkModelBridge(id<BookmarkModelBridgeObserver> observer,
                               bookmarks::BookmarkModel* model);
  ~BookmarkModelBridge() override;

 private:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  __weak id<BookmarkModelBridgeObserver> observer_;
  bookmarks::BookmarkModel* model_;  // weak
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
