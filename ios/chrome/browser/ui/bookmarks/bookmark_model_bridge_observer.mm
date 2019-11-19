// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace bookmarks {

BookmarkModelBridge::BookmarkModelBridge(
    id<BookmarkModelBridgeObserver> observer,
    BookmarkModel* model)
    : observer_(observer), model_(model) {
  DCHECK(observer_);
  DCHECK(model_);
  model_->AddObserver(this);
}

BookmarkModelBridge::~BookmarkModelBridge() {
  DCHECK(model_);
  model_->RemoveObserver(this);
}

void BookmarkModelBridge::BookmarkModelLoaded(BookmarkModel* model,
                                              bool ids_reassigned) {
  [observer_ bookmarkModelLoaded];
}

void BookmarkModelBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  // This is an inconsistent state in the application lifecycle. The bookmark
  // model shouldn't disappear.
  NOTREACHED();
}

void BookmarkModelBridge::BookmarkNodeMoved(BookmarkModel* model,
                                            const BookmarkNode* old_parent,
                                            size_t old_index,
                                            const BookmarkNode* new_parent,
                                            size_t new_index) {
  const BookmarkNode* node = new_parent->children()[new_index].get();
  [observer_ bookmarkNode:node movedFromParent:old_parent toParent:new_parent];
}

void BookmarkModelBridge::BookmarkNodeAdded(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            size_t index) {
  [observer_ bookmarkNodeChildrenChanged:parent];
}

void BookmarkModelBridge::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    size_t old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  [observer_ bookmarkNodeDeleted:node fromFolder:parent];
  [observer_ bookmarkNodeChildrenChanged:parent];
}

void BookmarkModelBridge::BookmarkNodeChanged(BookmarkModel* model,
                                              const BookmarkNode* node) {
  [observer_ bookmarkNodeChanged:node];
}

void BookmarkModelBridge::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  SEL selector = @selector(bookmarkNodeFaviconChanged:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ bookmarkNodeFaviconChanged:node];
  }
}

void BookmarkModelBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  [observer_ bookmarkNodeChildrenChanged:node];
}

void BookmarkModelBridge::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  [observer_ bookmarkModelRemovedAllNodes];
}

}  // namespace bookmarks
