// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mediator.h"

#import "base/auto_reset.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/prefs/pref_service.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_consumer.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "url/gurl.h"

@interface BookmarksEditorMediator () <BookmarkModelBridgeObserver,
                                       SyncObserverModelBridge> {
  raw_ptr<PrefService> _prefs;
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelObserver;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  base::WeakPtr<ProfileIOS> _profile;
  // Whether the user manually changed the folder. In which case it must be
  // saved as last used folder on "save".
  BOOL _manuallyChangedTheFolder;
}
// Flag to ignore bookmark model changes notifications.
// Property used in BookmarksEditorMutator
@property(nonatomic, assign) BOOL ignoresBookmarkModelChanges;

@end

@implementation BookmarksEditorMediator {
  base::WeakPtr<bookmarks::BookmarkModel> _bookmarkModel;
  raw_ptr<syncer::SyncService> _syncService;
  // The folder in which was the bookmark when the view was opened.
  raw_ptr<const bookmarks::BookmarkNode> _originalFolder;
  // Authentication service for this mediator.
  base::WeakPtr<AuthenticationService> _authenticationService;
}

- (instancetype)
    initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
             bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
                    prefs:(PrefService*)prefs
    authenticationService:(AuthenticationService*)authenticationService
              syncService:(syncer::SyncService*)syncService
                  profile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmarkModel->loaded());
    DCHECK(bookmarkNode);
    DCHECK(bookmarkNode->is_url()) << "Type: " << bookmarkNode->type();
    _bookmarkModel = bookmarkModel->AsWeakPtr();
    _bookmark = bookmarkNode;
    _folder = bookmarkNode->parent();
    _originalFolder = bookmarkNode->parent();
    _prefs = prefs;
    _bookmarkModelObserver.reset(
        new BookmarkModelBridge(self, _bookmarkModel.get()));
    _syncService = syncService;
    _syncObserverModelBridge.reset(new SyncObserverBridge(self, syncService));
    _profile = profile->AsWeakPtr();
    _authenticationService = authenticationService->GetWeakPtr();
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _bookmark = nullptr;
  _folder = nullptr;
  _prefs = nullptr;
  _bookmarkModelObserver.reset();
  _syncService = nullptr;
  _syncObserverModelBridge.reset();
  _profile = nullptr;
  _originalFolder = nullptr;
  _authenticationService = nullptr;
}

- (void)dealloc {
  DCHECK(!_bookmarkModel);
}

#pragma mark - Public

- (void)manuallyChangeFolder:(const bookmarks::BookmarkNode*)folder {
  _manuallyChangedTheFolder = YES;
  [self changeFolder:folder];
}

#pragma mark - BookmarksEditorMutator

- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder {
  return _folder && _bookmarkModel->IsLocalOnlyNode(*_folder) &&
         bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService);
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

- (void)bookmarkModelLoaded {
  // No-op.
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
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

- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }

  [self updateFolderLabel];
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  if (self.ignoresBookmarkModelChanges) {
    return;
  }

  if (self.bookmark == bookmarkNode) {
    [self.delegate bookmarkDidMoveToParent:newParent];
  }
}

- (void)willDeleteNode:(const bookmarks::BookmarkNode*)node
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
    //
    // In this case, fall back to the default folder, which is the mobile node
    // for the same storage type as before (local or account). With account
    // bookmarks, it is possible that permanent folders no longer exist (e.g.
    // the user signed out), which requires falling back to the local default.
    // back to the local model.
    if (_bookmarkModel->IsLocalOnlyNode(*self.folder) ||
        !_bookmarkModel->account_mobile_node() ||
        _bookmarkModel->account_mobile_node()->HasAncestor(node)) {
      [self changeFolder:_bookmarkModel->mobile_node()];
    } else {
      [self changeFolder:_bookmarkModel->account_mobile_node()];
    }
  }
}

- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  // No-op. Bookmark deletion handled in
  // `bookmarkModel:willDeleteNode:fromFolder:`
}

- (void)bookmarkModelRemovedAllNodes {
  // Nothing more to do.
}

- (void)bookmarkModelWillRemoveAllNodes {
  // The current node is going to be deleted.
  // Just close the view.
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
      showSnackbarMessage:bookmark_utils_ios::UpdateBookmarkWithUndoToast(
                              self.bookmark, name, url, _originalFolder,
                              self.folder, _bookmarkModel.get(), self.profile,
                              _authenticationService, _syncService)];
  if (_manuallyChangedTheFolder) {
    BookmarkStorageType type = bookmark_utils_ios::GetBookmarkStorageType(
        _folder, _bookmarkModel.get());
    SetLastUsedBookmarkFolder(_prefs, _folder, type);
  }
}

- (void)deleteBookmark {
  if (!(self.bookmark && _bookmarkModel->loaded())) {
    return;
  }
  // To stop getting recursive events from committed bookmark editing changes
  // ignore bookmark model updates notifications.
  base::AutoReset<BOOL> autoReset(&self->_ignoresBookmarkModelChanges, YES);

  // When launched from the star button, removing the current bookmark
  // removes all matching nodes.
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodesVector = _bookmarkModel->GetNodesByURL([self bookmark]->url());
  std::set<const bookmarks::BookmarkNode*> nodes(nodesVector.begin(),
                                                 nodesVector.end());
  if (!nodesVector.empty()) {
    // TODO (crbug.com/1445455): figure out why it is sometime empty and ensure
    // it is not the case.
    //  Temporary fix for crbug.com/1444667
    [self.snackbarCommandsHandler
        showSnackbarMessageOverBrowserToolbar:
            bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                nodes, _bookmarkModel.get(), self.profile, FROM_HERE)];
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

// Returns the profile.
- (ProfileIOS*)profile {
  return _profile.get();
}

@end
