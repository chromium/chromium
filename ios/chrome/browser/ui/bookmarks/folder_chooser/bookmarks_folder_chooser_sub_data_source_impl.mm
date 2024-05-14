// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserSubDataSourceImpl () <
    BookmarkModelBridgeObserver>
@end

@implementation BookmarksFolderChooserSubDataSourceImpl {
  // Bookmarks model object.
  raw_ptr<LegacyBookmarkModel> _bookmarkModel;
  // Observer for `_bookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  __weak id<BookmarksFolderChooserParentDataSource> _parentDataSource;
}

- (instancetype)initWithBookmarkModel:(LegacyBookmarkModel*)bookmarkModel
                     parentDataSource:
                         (id<BookmarksFolderChooserParentDataSource>)
                             parentDataSource {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  DCHECK(parentDataSource);

  self = [super init];
  if (self) {
    _bookmarkModel = bookmarkModel;
    _bookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, _bookmarkModel);
    _parentDataSource = parentDataSource;
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _bookmarkModelBridge = nil;
  _parentDataSource = nil;
}

- (void)dealloc {
  DCHECK(!_bookmarkModel);
}

#pragma mark - BookmarksFolderChooserSubDataSource

- (const BookmarkNode*)mobileFolderNode {
  return _bookmarkModel->mobile_node();
}

- (std::vector<const BookmarkNode*>)visibleFolderNodes {
  return bookmark_utils_ios::VisibleNonDescendantNodes(
      [_parentDataSource editedNodes], _bookmarkModel);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(LegacyBookmarkModel*)model {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED_IN_MIGRATION();
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [_consumer notifyModelUpdated];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [_parentDataSource bookmarkNodeDeleted:node];
  [_consumer notifyModelUpdated];
}

- (void)bookmarkModelWillRemoveAllNodes:(const LegacyBookmarkModel*)model {
  // `_consumer` is notified after the nodes are acutally deleted in
  // `bookmarkModelRemovedAllNodes`.
  [_parentDataSource bookmarkModelWillRemoveAllNodes:model];
}

- (void)bookmarkModelRemovedAllNodes:(LegacyBookmarkModel*)model {
  [_consumer notifyModelUpdated];
}

@end
