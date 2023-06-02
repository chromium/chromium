// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BookmarkModelBridge::BookmarkModelBridge(
    id<BookmarkModelBridgeObserver> observer,
    bookmarks::BookmarkModel* model)
    : observer_(observer) {
  DCHECK(observer_);
  DCHECK(model);
  model_observation_.Observe(model);
}

BookmarkModelBridge::~BookmarkModelBridge() {}

void BookmarkModelBridge::BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                                              bool ids_reassigned) {
  DCHECK(model_observation_.IsObservingSource(model));
  [observer_ bookmarkModelLoaded:model];
}

void BookmarkModelBridge::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  DCHECK(model_observation_.IsObservingSource(model));
  model_observation_.Reset();
}

void BookmarkModelBridge::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  DCHECK(model_observation_.IsObservingSource(model));
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  [observer_ bookmarkModel:model
               didMoveNode:node
                fromParent:old_parent
                  toParent:new_parent];
}

void BookmarkModelBridge::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  DCHECK(model_observation_.IsObservingSource(model));
  [observer_ bookmarkModel:model didChangeChildrenForNode:parent];
}

void BookmarkModelBridge::OnWillRemoveBookmarks(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node) {
  CHECK(model_observation_.IsObservingSource(model));
  SEL selector = @selector(bookmarkModel:willDeleteNode:fromFolder:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModel:model willDeleteNode:node fromFolder:parent];
  }
}

void BookmarkModelBridge::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  DCHECK(model_observation_.IsObservingSource(model));
  // Calling -bookmarkModel:didDeleteNode:fromFolder: may cause the current
  // bridge object to be destroyed, so code must not access `this` after (even
  // implictly), so copy `observer_` to a local variable. Use `__weak` for the
  // local variable, since BookmarkModelBridge uses a weak pointer already, so
  // if both observer and bridge are destroyed, we do not want to have the code
  // behave differently thank if only the observer was deallocated.
  __weak id<BookmarkModelBridgeObserver> observer = observer_;
  [observer bookmarkModel:model didDeleteNode:node fromFolder:parent];
  [observer bookmarkModel:model didChangeChildrenForNode:parent];
}

void BookmarkModelBridge::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  DCHECK(model_observation_.IsObservingSource(model));
  [observer_ bookmarkModel:model didChangeNode:node];
}

void BookmarkModelBridge::BookmarkNodeFaviconChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  DCHECK(model_observation_.IsObservingSource(model));
  SEL selector = @selector(bookmarkModel:didChangeFaviconForNode:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModel:model didChangeFaviconForNode:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChildrenReordered(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  DCHECK(model_observation_.IsObservingSource(model));
  [observer_ bookmarkModel:model didChangeChildrenForNode:node];
}

void BookmarkModelBridge::OnWillRemoveAllUserBookmarks(
    bookmarks::BookmarkModel* model) {
  DCHECK(model_observation_.IsObservingSource(model));
  SEL selector = @selector(bookmarkModelWillRemoveAllNodes:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModelWillRemoveAllNodes:model];
  }
}

void BookmarkModelBridge::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  DCHECK(model_observation_.IsObservingSource(model));
  [observer_ bookmarkModelRemovedAllNodes:model];
}
