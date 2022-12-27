// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "components/bookmarks/browser/bookmark_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BookmarkModelBridge::BookmarkModelBridge(
    id<BookmarkModelBridgeObserver> observer,
    bookmarks::BookmarkModel* model)
    : observer_(observer), model_(model) {
  DCHECK(observer_);
  DCHECK(model_);
  model_->AddObserver(this);
}

BookmarkModelBridge::~BookmarkModelBridge() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void BookmarkModelBridge::BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                                              bool ids_reassigned) {
  [observer_ bookmarkModelLoaded];
}

void BookmarkModelBridge::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  DCHECK(model_);
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void BookmarkModelBridge::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  [observer_ bookmarkNode:node movedFromParent:old_parent toParent:new_parent];
}

void BookmarkModelBridge::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  [observer_ bookmarkNodeChildrenChanged:parent];
}

void BookmarkModelBridge::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  // Hold a non-weak reference to `observer_`, in case the first event below
  // destroys `this`.
  id<BookmarkModelBridgeObserver> observer = observer_;

  [observer bookmarkNodeDeleted:node fromFolder:parent];
  [observer bookmarkNodeChildrenChanged:parent];
}

void BookmarkModelBridge::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  [observer_ bookmarkNodeChanged:node];
}

void BookmarkModelBridge::BookmarkNodeFaviconChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  SEL selector = @selector(bookmarkNodeFaviconChanged:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkNodeFaviconChanged:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChildrenReordered(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  [observer_ bookmarkNodeChildrenChanged:node];
}

void BookmarkModelBridge::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  [observer_ bookmarkModelRemovedAllNodes];
}
