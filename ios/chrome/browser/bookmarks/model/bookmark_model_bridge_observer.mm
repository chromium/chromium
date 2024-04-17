// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

BookmarkModelBridge::BookmarkModelBridge(
    id<BookmarkModelBridgeObserver> observer,
    LegacyBookmarkModel* model)
    : observer_(observer) {
  DCHECK(observer_);
  DCHECK(model);
  model_observation_.Observe(model);
}

BookmarkModelBridge::~BookmarkModelBridge() {}

void BookmarkModelBridge::BookmarkModelLoaded(bool ids_reassigned) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  [observer_ bookmarkModelLoaded:model];
}

void BookmarkModelBridge::BookmarkModelBeingDeleted() {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  model_observation_.Reset();

  SEL selector = @selector(bookmarkModelBeingDeleted:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModelBeingDeleted:model];
  }
}

void BookmarkModelBridge::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  [observer_ bookmarkModel:model
               didMoveNode:node
                fromParent:old_parent
                  toParent:new_parent];
}

void BookmarkModelBridge::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  [observer_ bookmarkModel:model didChangeChildrenForNode:parent];
  SEL selector = @selector(bookmarkModel:didAddNode:toFolder:);
  if ([observer_ respondsToSelector:selector]) {
    const bookmarks::BookmarkNode* node = parent->children()[index].get();
    [observer_ bookmarkModel:model didAddNode:node toFolder:parent];
  }
}

void BookmarkModelBridge::OnWillRemoveBookmarks(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  SEL selector = @selector(bookmarkModel:willDeleteNode:fromFolder:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModel:model willDeleteNode:node fromFolder:parent];
  }
}

void BookmarkModelBridge::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
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

void BookmarkModelBridge::OnWillChangeBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  SEL selector = @selector(bookmarkModel:willChangeBookmarkNode:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModel:model willChangeBookmarkNode:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  [observer_ bookmarkModel:model didChangeNode:node];
}

void BookmarkModelBridge::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  SEL selector = @selector(bookmarkModel:didChangeFaviconForNode:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModel:model didChangeFaviconForNode:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  [observer_ bookmarkModel:model didChangeChildrenForNode:node];
}

void BookmarkModelBridge::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  SEL selector = @selector(bookmarkModelWillRemoveAllNodes:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkModelWillRemoveAllNodes:model];
  }
}

void BookmarkModelBridge::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  LegacyBookmarkModel* model = model_observation_.GetSource();
  [observer_ bookmarkModelRemovedAllNodes:model];
}
