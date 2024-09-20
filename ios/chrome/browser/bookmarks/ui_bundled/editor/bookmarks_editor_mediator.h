// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mutator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AuthenticationService;
@protocol BookmarksEditorConsumer;
@protocol BookmarksEditorMediatorDelegate;
class PrefService;
@protocol SnackbarCommands;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for the bookmark editor
@interface BookmarksEditorMediator : NSObject <BookmarksEditorMutator>

// BookmarkNode to edit.
@property(nonatomic, readonly) const bookmarks::BookmarkNode* bookmark;
// Parent of `_bookmark` if the user tap on "save".
@property(nonatomic, assign) const bookmarks::BookmarkNode* folder;
// Delegate to change the view displayed.
@property(nonatomic, weak) id<BookmarksEditorMediatorDelegate> delegate;
// Consumer to reflect user’s change in the model.
@property(nonatomic, weak) id<BookmarksEditorConsumer> consumer;
// Handler for snackbar commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Designated initializer.
// `bookmarkModel` must not be `nullptr` and must be loaded. `bookmarkNode`
// mustn't be `nullptr` at initialization time. It also must be a URL. `prefs`
// is the user pref service.
- (instancetype)
    initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
             bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
                    prefs:(PrefService*)prefs
    authenticationService:(AuthenticationService*)authenticationService
              syncService:(syncer::SyncService*)syncService
                  profile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Changes `self.folder`, updates the UI accordingly.
// The change is not committed until the user taps the Save button.
// Save this folder as last used by user in preferences
// kIosBookmarkLastUsedFolderReceivingBookmarks and
// kIosBookmarkLastUsedStorageReceivingBookmarks on Save.
- (void)manuallyChangeFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_
