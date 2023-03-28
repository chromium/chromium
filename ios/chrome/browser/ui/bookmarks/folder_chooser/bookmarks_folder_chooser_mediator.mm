// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mediator.h"

#import "base/containers/contains.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mediator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mutator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserMediator () <
    BookmarksFolderChooserMutator,
    BookmarksFolderChooserParentDataSource,
    SyncObserverModelBridge>
@end

@implementation BookmarksFolderChooserMediator {
  // Data source from profile bookmark model;
  BookmarksFolderChooserSubDataSourceImpl* _profileDataSource;
  // Data source from account bookmark model;
  BookmarksFolderChooserSubDataSourceImpl* _accountDataSource;
  // List of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder. These are also the list of nodes that are
  // being edited (moved to a folder).
  std::set<const BookmarkNode*> _editedNodes;
  // Sync setup service indicates if the cloud slashed icon should be shown.
  SyncSetupService* _syncSetupService;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
}

- (instancetype)initWithBookmarkModel:(BookmarkModel*)model
                          editedNodes:(std::set<const BookmarkNode*>)nodes
                     syncSetupService:(SyncSetupService*)syncSetupService
                          syncService:(syncer::SyncService*)syncService {
  DCHECK(model);
  DCHECK(model->loaded());

  self = [super init];
  if (self) {
    _profileDataSource = [[BookmarksFolderChooserSubDataSourceImpl alloc]
        initWithBookmarkModel:model
             parentDataSource:self];
    // TODO(crbug.com/140237): Get account bookmark model and set account data
    // source here.
    _accountDataSource = nil;
    _editedNodes = std::move(nodes);
    _syncSetupService = syncSetupService;
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));
  }
  return self;
}

- (void)disconnect {
  [_profileDataSource disconnect];
  _profileDataSource = nil;
  [_accountDataSource disconnect];
  _accountDataSource = nil;
  _editedNodes.clear();
  _syncSetupService = nullptr;
  _syncObserverBridge = nullptr;
}

- (const std::set<const BookmarkNode*>&)editedNodes {
  return _editedNodes;
}

#pragma mark - BookmarksFolderChooserDataSource

- (id<BookmarksFolderChooserSubDataSource>)accountDataSource {
  return _accountDataSource;
}

- (id<BookmarksFolderChooserSubDataSource>)profileDataSource {
  return _profileDataSource;
}

- (BOOL)shouldDisplayCloudIconForProfileBookmarks {
  return bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
}

#pragma mark - BookmarksFolderChooserMutator

- (void)setSelectedFolderNode:(const BookmarkNode*)folderNode {
  _selectedFolderNode = folderNode;
  [_consumer notifyModelUpdated];
}

#pragma mark - BookmarksFolderChooserParentDataSource

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode {
  // Remove node from `_editedNodes` if it is already deleted (possibly remotely
  // by another sync device).
  if (base::Contains(_editedNodes, bookmarkNode)) {
    _editedNodes.erase(bookmarkNode);
    // if `_editedNodes` becomes empty, nothing to move.  Exit the folder
    // chooser.
    if (_editedNodes.empty()) {
      [_delegate bookmarksFolderChooserMediatorWantsDismissal:self];
    }
    // Exit here because no visible node was deleted. Nodes in `_editedNodes`
    // cannot be any visible folder in folder chooser.
    return;
  }

  if (!bookmarkNode->is_folder()) {
    return;
  }

  if (bookmarkNode == _selectedFolderNode) {
    // The selected folder has been deleted. Unset `_selectedFolderNode`.
    _selectedFolderNode = nil;
  }
  [_consumer notifyModelUpdated];
}

- (void)bookmarkModelWillRemoveAllNodes:(const BookmarkModel*)bookmarkModel {
  auto nodeInModel = [bookmarkModel](const BookmarkNode* node) {
    return node->HasAncestor(bookmarkModel->root_node());
  };
  // Remove will-be removed nodes (in `model`) from `_editedNodes`.
  std::erase_if(_editedNodes, nodeInModel);

  if (_editedNodes.empty()) {
    // if `_editedNodes` becomes empty, nothing to move.  Exit the folder
    // chooser.
    [_delegate bookmarksFolderChooserMediatorWantsDismissal:self];
  } else if (_selectedFolderNode->HasAncestor(bookmarkModel->root_node())) {
    // The selected folder will be deleted. Unset `_selectedFolderNode`. The UI
    // will be updated after the nodes are deleted in
    // `BookmarksFolderChooserSubDataSourceImpl::bookmarkModelRemovedAllNodes`.
    _selectedFolderNode = nil;
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer notifyModelUpdated];
}

#pragma mark - Property

- (void)setConsumer:(id<BookmarksFolderChooserConsumer>)consumer {
  _consumer = consumer;
  _profileDataSource.consumer = consumer;
  _accountDataSource.consumer = consumer;
}

@end
