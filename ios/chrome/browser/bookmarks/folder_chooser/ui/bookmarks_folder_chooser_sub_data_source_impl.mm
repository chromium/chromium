// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/folder_chooser/ui/bookmarks_folder_chooser_sub_data_source_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"

using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserSubDataSourceImpl () <
    BookmarkModelBridgeObserver>
@end

@implementation BookmarksFolderChooserSubDataSourceImpl {
  // Bookmarks model object.
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  // Which subset of the BookmarkModel is in scope of this data source.
  BookmarkStorageType _type;
  // Observer for `_bookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  __weak id<BookmarksFolderChooserParentDataSource> _parentDataSource;
}

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                                 type:(BookmarkStorageType)type
                     parentDataSource:
                         (id<BookmarksFolderChooserParentDataSource>)
                             parentDataSource {
  CHECK(bookmarkModel, base::NotFatalUntil::M152);
  CHECK(bookmarkModel->loaded(), base::NotFatalUntil::M152);
  CHECK(parentDataSource, base::NotFatalUntil::M152);

  self = [super init];
  if (self) {
    _bookmarkModel = bookmarkModel;
    _type = type;
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
  CHECK(!_bookmarkModel, base::NotFatalUntil::M152);
}

#pragma mark - BookmarksFolderChooserSubDataSource

- (const BookmarkNode*)mobileFolderNode {
  switch (_type) {
    case BookmarkStorageType::kLocalOrSyncable:
      return _bookmarkModel->mobile_node();
    case BookmarkStorageType::kAccount:
      return _bookmarkModel->account_mobile_node();
  }
  NOTREACHED(base::NotFatalUntil::M152);
}

- (std::vector<const BookmarkNode*>)visibleFolderNodes {
  return bookmark_utils_ios::VisibleNonDescendantNodes(
      [_parentDataSource editedNodes], _bookmarkModel, _type);
}

- (std::vector<const bookmarks::BookmarkNode*>)visibleFolderNodesForQuery:
    (const bookmarks::QueryFields&)query {
  std::vector<std::u16string> words = bookmarks::ParseBookmarkQuery(query);
  return bookmark_utils_ios::VisibleNonDescendantNodes(
      [_parentDataSource editedNodes], _bookmarkModel, _type, words);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED(base::NotFatalUntil::M152);
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [_consumer notifyModelUpdated];
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {
    [_consumer notifyModelUpdated];
  }
}

- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [_parentDataSource bookmarkNodeDeleted:node];
  [_consumer notifyModelUpdated];
}

- (void)bookmarkModelWillRemoveAllNodes {
  // `_consumer` is notified after the nodes are acutally deleted in
  // `bookmarkModelRemovedAllNodes`.
  [_parentDataSource bookmarkModelWillRemoveAllNodes];
}

- (void)bookmarkModelRemovedAllNodes {
  [_consumer notifyModelUpdated];
}

@end
