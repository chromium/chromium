// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mediator.h"

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mediator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mutator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserMediator () <
    AuthenticationServiceObserving,
    BookmarksFolderChooserMutator,
    BookmarksFolderChooserParentDataSource,
    SyncObserverModelBridge>
@end

@implementation BookmarksFolderChooserMediator {
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  // Data source from local bookmark nodes.
  BookmarksFolderChooserSubDataSourceImpl* _localOrSyncableDataSource;
  // Data source from account bookmark nodes.
  BookmarksFolderChooserSubDataSourceImpl* _accountDataSource;
  // Set of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder. These are also the list of nodes that are
  // being edited (moved to a folder).
  std::set<const BookmarkNode*> _editedNodes;
  // Observer for signin status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge> _authServiceBridge;
  // Sync service.
  raw_ptr<syncer::SyncService> _syncService;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
}

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)model
                          editedNodes:(std::set<const BookmarkNode*>)editedNodes
                authenticationService:(AuthenticationService*)authService
                          syncService:(syncer::SyncService*)syncService {
  DCHECK(model);
  DCHECK(model->loaded());
  DCHECK(authService->initialized());

  self = [super init];
  if (self) {
    _bookmarkModel = model;
    _localOrSyncableDataSource =
        [[BookmarksFolderChooserSubDataSourceImpl alloc]
            initWithBookmarkModel:model
                             type:BookmarkStorageType::kLocalOrSyncable
                 parentDataSource:self];
    _accountDataSource = [[BookmarksFolderChooserSubDataSourceImpl alloc]
        initWithBookmarkModel:model
                         type:BookmarkStorageType::kAccount
             parentDataSource:self];

    _editedNodes = std::move(editedNodes);
    _authServiceBridge = std::make_unique<AuthenticationServiceObserverBridge>(
        authService, self);
    _syncService = syncService;
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));
  }
  return self;
}

- (void)disconnect {
  [_localOrSyncableDataSource disconnect];
  _localOrSyncableDataSource.consumer = nil;
  _localOrSyncableDataSource = nil;
  [_accountDataSource disconnect];
  _accountDataSource.consumer = nil;
  _accountDataSource = nil;
  _editedNodes.clear();
  _authServiceBridge.reset();
  _syncService = nullptr;
  _syncObserverBridge = nullptr;
}

- (void)dealloc {
  DCHECK(!_localOrSyncableDataSource);
}

- (const std::set<const BookmarkNode*>&)editedNodes {
  return _editedNodes;
}

#pragma mark - BookmarksFolderChooserDataSource

- (id<BookmarksFolderChooserSubDataSource>)accountDataSource {
  DCHECK([self shouldShowAccountBookmarks]);
  return _accountDataSource;
}

- (id<BookmarksFolderChooserSubDataSource>)localOrSyncableDataSource {
  return _localOrSyncableDataSource;
}

- (BOOL)shouldDisplayCloudIconForLocalOrSyncableBookmarks {
  return bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService);
}

- (BOOL)shouldShowAccountBookmarks {
  return bookmark_utils_ios::IsAccountBookmarkStorageAvailable(_bookmarkModel);
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
}

- (void)bookmarkModelWillRemoveAllNodes {
  _editedNodes.clear();
  _selectedFolderNode = nil;
  // Nothing to move so exit the folder chooser.
  [_delegate bookmarksFolderChooserMediatorWantsDismissal:self];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [_consumer notifyModelUpdated];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer notifyModelUpdated];
}

#pragma mark - Property

- (void)setConsumer:(id<BookmarksFolderChooserConsumer>)consumer {
  _consumer = consumer;
  _localOrSyncableDataSource.consumer = consumer;
  _accountDataSource.consumer = consumer;
}

@end
