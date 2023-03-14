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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserMediator () <BookmarksFolderChooserMutator,
                                              BookmarkModelBridgeObserver,
                                              SyncObserverModelBridge>
@end

@implementation BookmarksFolderChooserMediator {
  // Model object that holds all bookmarks.
  BookmarkModel* _bookmarkModel;
  // Observer for `_bookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _modelBridge;
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
    _bookmarkModel = model;
    _modelBridge.reset(new BookmarkModelBridge(self, _bookmarkModel));
    _editedNodes = std::move(nodes);
    _syncSetupService = syncSetupService;
    _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _modelBridge = nil;
  _editedNodes.clear();
  _syncSetupService = nullptr;
  _syncObserverBridge = nullptr;
}

- (const std::set<const BookmarkNode*>&)editedNodes {
  return _editedNodes;
}

#pragma mark - BookmarksFolderChooserDataSource

- (const BookmarkNode*)rootFolder {
  return _bookmarkModel->root_node();
}

- (BOOL)shouldDisplayCloudIconForProfileBookmarks {
  return bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
}

- (std::vector<const BookmarkNode*>)visibleFolders {
  return bookmark_utils_ios::VisibleNonDescendantNodes(_editedNodes,
                                                       _bookmarkModel);
}

#pragma mark - BookmarksFolderChooserMutator

- (void)setSelectedFolder:(const BookmarkNode*)folder {
  _selectedFolder = folder;
  [_consumer notifyModelUpdated];
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

  if (bookmarkNode == _selectedFolder) {
    // The selected folder has been deleted. Fallback on the Mobile Bookmarks
    // node.
    _selectedFolder = _bookmarkModel->mobile_node();
  }
  [_consumer notifyModelUpdated];
}

- (void)bookmarkModelRemovedAllNodes {
  // The selected folder is no longer valid. Fallback on the Mobile Bookmarks
  // node.
  _selectedFolder = _bookmarkModel->mobile_node();
  [_consumer notifyModelUpdated];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer notifyModelUpdated];
}

@end
