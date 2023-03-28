// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserSubDataSourceImpl () <
    BookmarkModelBridgeObserver>
@end

@implementation BookmarksFolderChooserSubDataSourceImpl {
  // Bookmarks model object.
  BookmarkModel* _bookmarkModel;
  // Observer for `_bookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  __weak id<BookmarksFolderChooserParentDataSource> _parentDataSource;
}

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
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

#pragma mark - BookmarksFolderChooserSubDataSource

- (const BookmarkNode*)mobileFolderNode {
  return _bookmarkModel->mobile_node();
}

- (const BookmarkNode*)rootFolderNode {
  return _bookmarkModel->root_node();
}

- (std::vector<const BookmarkNode*>)visibleFolderNodes {
  return bookmark_utils_ios::VisibleNonDescendantNodes(
      [_parentDataSource editedNodes], _bookmarkModel);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [_consumer notifyModelUpdated];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  [_parentDataSource bookmarkNodeDeleted:bookmarkNode];
}

- (void)bookmarkModelWillRemoveAllNodes:(const BookmarkModel*)model {
  [_parentDataSource bookmarkModelWillRemoveAllNodes:model];
}

- (void)bookmarkModelRemovedAllNodes {
  [_consumer notifyModelUpdated];
}

@end
