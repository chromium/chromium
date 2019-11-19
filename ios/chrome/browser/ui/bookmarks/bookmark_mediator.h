// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class GURL;
@class MDCSnackbarMessage;

// Mediator for the bookmarks.
@interface BookmarkMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Accesses the default folder for bookmarks. The default folder is Mobile
// Bookmarks.
+ (const bookmarks::BookmarkNode*)folderForNewBookmarksInBrowserState:
    (ios::ChromeBrowserState*)browserState;
+ (void)setFolderForNewBookmarks:(const bookmarks::BookmarkNode*)folder
                  inBrowserState:(ios::ChromeBrowserState*)browserState;

// Adds a bookmark with a |title| and a |URL| and display a snackbar with an
// |editAction|. Returns a message to be displayed after the Bookmark has been
// added.
- (MDCSnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                        URL:(const GURL&)URL
                                 editAction:(void (^)())editAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MEDIATOR_H_
