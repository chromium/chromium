// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "components/bookmarks/browser/bookmark_node.h"

BookmarkModelBridge::BookmarkModelBridge(
    id<BookmarkModelBridgeObserver> observer,
    bookmarks::BookmarkModel* model)
    : observer_(observer) {
  DCHECK(observer_);
  DCHECK(model);
  model_observation_.Observe(model);
}

BookmarkModelBridge::~BookmarkModelBridge() {}

void BookmarkModelBridge::BookmarkModelLoaded(bool ids_reassigned) {
  [observer_ bookmarkModelLoaded];
}

void BookmarkModelBridge::BookmarkModelBeingDeleted() {
  model_observation_.Reset();

  SEL selector = @selector(bookmarkModelBeingDeleted);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModelBeingDeleted];
  }
}

void BookmarkModelBridge::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  [observer_ didMoveNode:node fromParent:old_parent toParent:new_parent];
}

void BookmarkModelBridge::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  [observer_ didChangeChildrenForNode:parent];
  SEL selector = @selector(didAddNode:toFolder:);
  if ([observer_ respondsToSelector:selector]) {
    const bookmarks::BookmarkNode* node = parent->children()[index].get();
    [observer_ didAddNode:node toFolder:parent];
  }
}

void BookmarkModelBridge::OnWillRemoveBookmarks(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  SEL selector = @selector(willDeleteNode:fromFolder:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ willDeleteNode:node fromFolder:parent];
  }
}

void BookmarkModelBridge::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // Calling -didDeleteNode:fromFolder: may cause the current
  // bridge object to be destroyed, so code must not access `this` after (even
  // implictly), so copy `observer_` to a local variable. Use `__weak` for the
  // local variable, since BookmarkModelBridge uses a weak pointer already, so
  // if both observer and bridge are destroyed, we do not want to have the code
  // behave differently thank if only the observer was deallocated.
  __weak id<BookmarkModelBridgeObserver> observer = observer_;
  [observer didDeleteNode:node fromFolder:parent];
  [observer didChangeChildrenForNode:parent];
}

void BookmarkModelBridge::OnWillChangeBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  SEL selector = @selector(willChangeBookmarkNode:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ willChangeBookmarkNode:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  [observer_ didChangeNode:node];
}

void BookmarkModelBridge::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  SEL selector = @selector(didChangeFaviconForNode:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ didChangeFaviconForNode:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  [observer_ didChangeChildrenForNode:node];
}

void BookmarkModelBridge::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  SEL selector = @selector(bookmarkModelWillRemoveAllNodes);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModelWillRemoveAllNodes];
  }
}

void BookmarkModelBridge::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  [observer_ bookmarkModelRemovedAllNodes];
}
