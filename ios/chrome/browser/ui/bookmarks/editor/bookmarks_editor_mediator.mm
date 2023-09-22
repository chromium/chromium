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
#import "components/sync/base/features.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator_delegate.h"
#import "url/gurl.h"

@interface BookmarksEditorMediator () <BookmarkModelBridgeObserver,
                                       SyncObserverModelBridge> {
  PrefService* _prefs;

  // Observer for the bookmark model of `self.bookmark`.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridgeObserver;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  ChromeBrowserState* _browserState;
  // Whether the user manually changed the folder. In which case it must be
  // saved as last used folder on "save".
  BOOL _manuallyChangedTheFolder;
}
// Flag to ignore bookmark model changes notifications.
// Property used in BookmarksEditorMutator
@property(nonatomic, assign) BOOL ignoresBookmarkModelChanges;

@end

@implementation BookmarksEditorMediator {
  base::WeakPtr<bookmarks::BookmarkModel> _localOrSyncableBookmarkModel;
  base::WeakPtr<bookmarks::BookmarkModel> _accountBookmarkModel;
  syncer::SyncService* _syncService;
}

- (instancetype)
    initWithLocalOrSyncableBookmarkModel:
        (bookmarks::BookmarkModel*)localOrSyncableBookmarkModel
                    accountBookmarkModel:
                        (bookmarks::BookmarkModel*)accountBookmarkModel
                            bookmarkNode:
                                (const bookmarks::BookmarkNode*)bookmarkNode
                                   prefs:(PrefService*)prefs
                             syncService:(syncer::SyncService*)syncService
                            browserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(localOrSyncableBookmarkModel);
    DCHECK(localOrSyncableBookmarkModel->loaded());
    if (base::FeatureList::IsEnabled(syncer::kEnableBookmarksAccountStorage)) {
      DCHECK(accountBookmarkModel);
      DCHECK(accountBookmarkModel->loaded());
    } else {
      DCHECK(!accountBookmarkModel);
    }
    DCHECK(bookmarkNode);
    DCHECK(bookmarkNode->is_url()) << "Type: " << bookmarkNode->type();
    _localOrSyncableBookmarkModel = localOrSyncableBookmarkModel->AsWeakPtr();
    if (accountBookmarkModel) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
    }
    _bookmark = bookmarkNode;
    _folder = bookmarkNode->parent();
    _prefs = prefs;
    _bookmarkModelBridgeObserver.reset(
        new BookmarkModelBridge(self, self.bookmarkModel));
    _syncService = syncService;
    _syncObserverModelBridge.reset(new SyncObserverBridge(self, syncService));
    _browserState = browserState;
  }
  return self;
}

- (void)disconnect {
  _localOrSyncableBookmarkModel = nullptr;
  _accountBookmarkModel = nullptr;
  _bookmark = nullptr;
  _folder = nullptr;
  _prefs = nullptr;
  _bookmarkModelBridgeObserver.reset();
  _syncService = nullptr;
  _syncObserverModelBridge.reset();
  _browserState = nullptr;
}

- (void)dealloc {
  DCHECK(!_localOrSyncableBookmarkModel);
}

#pragma mark - Public

- (void)manuallyChangeFolder:(const bookmarks::BookmarkNode*)folder {
  _manuallyChangedTheFolder = YES;
  [self changeFolder:folder];
}

#pragma mark - Properties

- (bookmarks::BookmarkModel*)bookmarkModel {
  return bookmark_utils_ios::GetBookmarkModelForNode(
      self.bookmark, _localOrSyncableBookmarkModel.get(),
      _accountBookmarkModel.get());
}

#pragma mark - BookmarksEditorMutator

- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder {
  bookmarks::StorageType type = bookmark_utils_ios::GetBookmarkModelType(
      self.folder, _localOrSyncableBookmarkModel.get(),
      _accountBookmarkModel.get());
  switch (type) {
    case bookmarks::StorageType::kLocalOrSyncable:
      return bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService);
    case bookmarks::StorageType::kAccount:
      return NO;
  }
  NOTREACHED_NORETURN();
}

#pragma mark - Private

// Change the folder of this editor and update the view.
- (void)changeFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  [self setFolder:folder];
  [self updateFolderLabel];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  // No-op.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }
  // If the changed bookmark is not the current one.
  if (self.bookmark == bookmarkNode) {
    return;
  }
  [self.consumer
      updateUIWithName:bookmark_utils_ios::TitleForBookmarkNode(_bookmark)
                   URL:base::SysUTF8ToNSString(_bookmark->url().spec())
            folderName:bookmark_utils_ios::TitleForBookmarkNode(_folder)];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }

  [self updateFolderLabel];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.delegate bookmarkDidMoveToParent:newParent];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
       willDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark->HasAncestor(node)) {
    _bookmark = nullptr;
    [self.delegate bookmarkEditorMediatorWantsDismissal:self];
  } else if (self.folder->HasAncestor(node)) {
    // This might happen when the user has changed `self.folder` but has not
    // commited the changes by pressing done. And in the background the chosen
    // folder was deleted.
    [self changeFolder:model->mobile_node()];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  // No-op. Bookmark deletion handled in
  // `bookmarkModel:willDeleteNode:fromFolder:`
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  CHECK(!self.ignoresBookmarkModelChanges);
  _bookmark = nullptr;
  self.folder = nullptr;
  [self.delegate bookmarkEditorMediatorWantsDismissal:self];
}

#pragma mark - BookmarksEditorMutator

- (void)commitBookmarkChangesWithURLString:(NSString*)URLString
                                      name:(NSString*)name {
  // To stop getting recursive events from committed bookmark editing changes
  // ignore bookmark model updates notifications.
  base::AutoReset<BOOL> autoReset(&self->_ignoresBookmarkModelChanges, YES);

  GURL url = bookmark_utils_ios::ConvertUserDataToGURL(URLString);
  // If the URL was not valid, the `save` message shouldn't have been sent.
  DCHECK(url.is_valid());

  // Tell delegate if bookmark name or title has been changed.
  if ([self bookmark] &&
      ([self bookmark]->GetTitle() != base::SysNSStringToUTF16(name) ||
       [self bookmark]->url() != url)) {
    [self.delegate bookmarkEditorWillCommitTitleOrURLChange:self];
  }

  [self.snackbarCommandsHandler
      showSnackbarMessage:
          bookmark_utils_ios::CreateOrUpdateBookmarkWithUndoToast(
              [self bookmark], name, url, [self folder],
              _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get(),
              _browserState)];
  if (_manuallyChangedTheFolder) {
    bookmarks::StorageType type = bookmark_utils_ios::GetBookmarkModelType(
        _folder, _localOrSyncableBookmarkModel.get(),
        _accountBookmarkModel.get());
    SetLastUsedBookmarkFolder(_prefs, _folder, type);
  }
}

- (void)deleteBookmark {
  if (!(self.bookmark && self.bookmarkModel->loaded())) {
    return;
  }
  // To stop getting recursive events from committed bookmark editing changes
  // ignore bookmark model updates notifications.
  base::AutoReset<BOOL> autoReset(&self->_ignoresBookmarkModelChanges, YES);

  // When launched from the star button, removing the current bookmark
  // removes all matching nodes.
  std::vector<const bookmarks::BookmarkNode*> nodesVector;
  [self bookmarkModel]->GetNodesByURL([self bookmark]->url(), &nodesVector);
  std::set<const bookmarks::BookmarkNode*> nodes(nodesVector.begin(),
                                                 nodesVector.end());
  if (!nodesVector.empty()) {
    // TODO (crbug.com/1445455): figure out why it is sometime empty and ensure
    // it is not the case.
    //  Temporary fix for crbug.com/1444667
    [self.snackbarCommandsHandler
        showSnackbarMessageOverBrowserToolbar:
            bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                nodes, {[self bookmarkModel]}, _browserState)];
    [self.delegate bookmarkEditorMediatorWantsDismissal:self];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer updateSync];
}

#pragma mark - Private

// Tells the consumer to update the name of the bookmark’s folder.
- (void)updateFolderLabel {
  NSString* folderName = @"";
  if (_bookmark) {
    folderName = bookmark_utils_ios::TitleForBookmarkNode(_folder);
  }
  [_consumer updateFolderLabel:folderName];
}

@end
