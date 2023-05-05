// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_

#import <UIKit/UIKit.h>

class AuthenticationService;
class GURL;
@class MDCSnackbarMessage;
class PrefService;
class SyncSetupService;
@class URLWithTitle;

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Mediator for the bookmarks.
@interface BookmarkMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)
    initWithWithProfileBookmarkModel:
        (bookmarks::BookmarkModel*)profileBookmarkModel
                accountBookmarkModel:
                    (bookmarks::BookmarkModel*)accountBookmarkModel
                               prefs:(PrefService*)prefs
               authenticationService:
                   (AuthenticationService*)authenticationService
                         syncService:(syncer::SyncService*)syncService
                    syncSetupService:(SyncSetupService*)syncSetupService
    NS_DESIGNATED_INITIALIZER;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Disconnects the mediator
- (void)disconnect;

// Adds a bookmark with a `title` and a `URL` and display a snackbar with an
// `editAction`. Returns a message to be displayed after the Bookmark has been
// added.
- (MDCSnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                        URL:(const GURL&)URL
                                 editAction:(void (^)())editAction;

// Adds bookmarks for `URLs` into `folder`. Returns a message to be displayed
// after the Bookmark has been added.
- (MDCSnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                           toFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_
