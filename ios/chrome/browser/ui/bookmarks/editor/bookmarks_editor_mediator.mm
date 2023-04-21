// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator.h"
#import "base/auto_reset.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
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

@implementation BookmarksEditorMediator {
  base::WeakPtr<bookmarks::BookmarkModel> _profileBookmarkModel;
  base::WeakPtr<bookmarks::BookmarkModel> _accountBookmarkModel;
}

- (instancetype)
    initWithProfileBookmarkModel:(bookmarks::BookmarkModel*)profileBookmarkModel
            accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
                    bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
                           prefs:(PrefService*)prefs
                syncSetupService:(SyncSetupService*)syncSetupService
                     syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    DCHECK(profileBookmarkModel);
    DCHECK(profileBookmarkModel->loaded());
    if (base::FeatureList::IsEnabled(
            bookmarks::kEnableBookmarksAccountStorage)) {
      DCHECK(accountBookmarkModel);
      DCHECK(accountBookmarkModel->loaded());
    } else {
      DCHECK(!accountBookmarkModel);
    }
    DCHECK(bookmarkNode);
    DCHECK(bookmarkNode->is_url()) << "Type: " << bookmarkNode->type();
    _profileBookmarkModel = profileBookmarkModel->AsWeakPtr();
    if (accountBookmarkModel) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
    }
    _bookmark = bookmarkNode;
    _folder = bookmarkNode->parent();
    _prefs = prefs;
    _bookmarkModelBridgeObserver.reset(
        new BookmarkModelBridge(self, self.bookmarkModel));
    _syncObserverModelBridge.reset(new SyncObserverBridge(self, syncService));
    _syncSetupService = syncSetupService;
  }
  return self;
}

- (void)disconnect {
  _profileBookmarkModel = nullptr;
  _accountBookmarkModel = nullptr;
  _bookmark = nullptr;
  _folder = nullptr;
  _prefs = nullptr;
  _bookmarkModelBridgeObserver = nullptr;
  _syncObserverModelBridge = nullptr;
}

#pragma mark - Properties

- (bookmarks::BookmarkModel*)bookmarkModel {
  return bookmark_utils_ios::GetBookmarkModelForNode(
      self.bookmark, _profileBookmarkModel.get(), _accountBookmarkModel.get());
}

#pragma mark - BookmarksEditorMutator

- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder {
  BookmarkModelType type = bookmark_utils_ios::GetBookmarkModelType(
      self.bookmark, _profileBookmarkModel.get(), _accountBookmarkModel.get());
  switch (type) {
    case BookmarkModelType::kProfile:
      return bookmark_utils_ios::ShouldDisplayCloudSlashIconForProfileModel(
          _syncSetupService);
    case BookmarkModelType::kAccount:
      return NO;
  }
  NOTREACHED_NORETURN();
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

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  // No-op.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.consumer updateUIFromBookmark];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  [self.consumer updateFolderLabel];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.delegate bookmarkDidMoveToParent:newParent];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (_ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == node) {
    self.bookmark = nullptr;
    [self.delegate bookmarkEditorMediatorWantsDismissal:self];
  } else if (self.folder == node) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  CHECK(!_ignoresBookmarkModelChanges);
  self.bookmark = nullptr;
  self.folder = nullptr;
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
