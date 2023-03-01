// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator.h"
#import "base/auto_reset.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/prefs/pref_service.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator_delegate.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksEditorMediator () <BookmarkModelBridgeObserver,
                                       SyncObserverModelBridge> {
  PrefService* _prefs;

  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridgeObserver;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  SyncSetupService* _syncSetupService;
}
// Flag to ignore bookmark model changes notifications.
@property(nonatomic, assign) BOOL ignoresBookmarkModelChanges;

@end

@implementation BookmarksEditorMediator

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                             bookmark:(const bookmarks::BookmarkNode*)bookmark
                                prefs:(PrefService*)prefs
                     syncSetupService:(SyncSetupService*)syncSetupService
                          syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());
    DCHECK(bookmarkModel->loaded());
    _bookmarkModel = bookmarkModel;
    _bookmark = bookmark;
    _folder = bookmark->parent();
    _prefs = prefs;
    _bookmarkModelBridgeObserver.reset(
        new BookmarkModelBridge(self, self.bookmarkModel));
    _syncObserverModelBridge.reset(new SyncObserverBridge(self, syncService));
    _syncSetupService = syncSetupService;
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _bookmark = nullptr;
  _folder = nullptr;
  _bookmarkModelBridgeObserver = nil;
  _syncObserverModelBridge = nil;
}

#pragma mark - BookmarksEditorMutator

- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder {
  return bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
}

- (void)changeFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  [self setFolder:folder];
  // TODO:(crbug.com/1411901): update kIosBookmarkFolderDefault on save only.
  _prefs->SetInt64(prefs::kIosBookmarkFolderDefault, folder->id());
  [self.consumer updateFolderLabel];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // No-op.
}

- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.consumer updateUIFromBookmark];
  }
}

- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  [self.consumer updateFolderLabel];
}

- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.delegate bookmarkDidMoveToParent:newParent];
  }
}

- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)bookmarkNode
                 fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    self.bookmark = nil;
    [self.delegate bookmarkEditorMediatorWantsDismissal:self];
  } else if (self.folder == bookmarkNode) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  self.bookmark = nil;
  if (!self.bookmarkModel->is_permanent_node(self.folder)) {
    // TODO(crbug.com/1404311) Do not edit the bookmark that has been deleted.
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }

  [self.delegate bookmarkEditorMediatorWantsDismissal:self];
}

#pragma mark - BookmarksEditorMutator

- (BOOL*)ignoresBookmarkModelChangesPointer {
  return &_ignoresBookmarkModelChanges;
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer updateSync];
}

@end
