// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mediator.h"

#import "base/containers/contains.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mediator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mutator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserMediator () <
    AuthenticationServiceObserving,
    BookmarksFolderChooserMutator,
    BookmarksFolderChooserParentDataSource,
    SyncObserverModelBridge>
@end

@implementation BookmarksFolderChooserMediator {
  // Data source from localOrSyncable bookmark model;
  BookmarksFolderChooserSubDataSourceImpl* _localOrSyncableDataSource;
  // Data source from account bookmark model;
  BookmarksFolderChooserSubDataSourceImpl* _accountDataSource;
  // Set of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder. These are also the list of nodes that are
  // being edited (moved to a folder). This set may contain nodes from both the
  // `_localOrSyncableBookmarkModel` and `_accountBookmarkModel`.
  std::set<const BookmarkNode*> _editedNodes;
  // Observer for signin status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge> _authServiceBridge;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
}

- (instancetype)
    initWithLocalOrSyncableBookmarkModel:
        (BookmarkModel*)localOrSyncableBookmarkModel
                    accountBookmarkModel:(BookmarkModel*)accountBookmarkModel
                             editedNodes:
                                 (std::set<const BookmarkNode*>)editedNodes
                   authenticationService:(AuthenticationService*)authService
                             syncService:(syncer::SyncService*)syncService {
  DCHECK(localOrSyncableBookmarkModel);
  DCHECK(localOrSyncableBookmarkModel->loaded());
  if (base::FeatureList::IsEnabled(syncer::kEnableBookmarksAccountStorage)) {
    DCHECK(accountBookmarkModel);
    DCHECK(accountBookmarkModel->loaded());
  } else {
    DCHECK(!accountBookmarkModel);
  }
  DCHECK(authService->initialized());

  self = [super init];
  if (self) {
    _localOrSyncableDataSource =
        [[BookmarksFolderChooserSubDataSourceImpl alloc]
            initWithBookmarkModel:localOrSyncableBookmarkModel
                 parentDataSource:self];
    if (accountBookmarkModel) {
      _accountDataSource = [[BookmarksFolderChooserSubDataSourceImpl alloc]
          initWithBookmarkModel:accountBookmarkModel
               parentDataSource:self];
    }
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
  return bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService);
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
    // The selected folder will be deleted. Unset `_selectedFolderNode`.
    _selectedFolderNode = nil;
  }
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
