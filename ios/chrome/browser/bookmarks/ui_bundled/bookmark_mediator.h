// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_MEDIATOR_H_

#import <UIKit/UIKit.h>

class AuthenticationService;
class GURL;
@class MDCSnackbarMessage;
class PrefService;
@class URLWithTitle;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Mediator for the bookmarks.
@interface BookmarkMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                                prefs:(PrefService*)prefs
                authenticationService:
                    (AuthenticationService*)authenticationService
                          syncService:(syncer::SyncService*)syncService
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

// Bulk adds URLs to bookmarks by automatically using their hostname + path as
// title. Returns a snackbar toast message with the amount of bookmarks
// successfully added and with the viewAction passed. Skips adding invalid URLs
// or URLs already bookmarked.
- (MDCSnackbarMessage*)bulkAddBookmarksWithURLs:(NSArray<NSURL*>*)URLs
                                     viewAction:(void (^)())viewAction;

// Adds bookmarks for `URLs` into `folder`. Returns a message to be displayed
// after the Bookmark has been added.
- (MDCSnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                           toFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_MEDIATOR_H_
