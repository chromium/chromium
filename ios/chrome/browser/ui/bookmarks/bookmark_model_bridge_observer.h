// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol BookmarkModelBridgeObserver<NSObject>
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
// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder;
// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes;

@optional
// The node favicon changed.
- (void)bookmarkNodeFaviconChanged:(const bookmarks::BookmarkNode*)bookmarkNode;
@end

namespace bookmarks {
// A bridge that translates BookmarkModelObserver C++ callbacks into ObjC
// callbacks.
class BookmarkModelBridge : public BookmarkModelObserver {
 public:
  explicit BookmarkModelBridge(id<BookmarkModelBridgeObserver> observer,
                               BookmarkModel* model);
  ~BookmarkModelBridge() override;

 private:
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(BookmarkModel* model) override;
  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index) override;
  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  __weak id<BookmarkModelBridgeObserver> observer_;
  BookmarkModel* model_;  // weak
};
}  // namespace bookmarks

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MODEL_BRIDGE_OBSERVER_H_
