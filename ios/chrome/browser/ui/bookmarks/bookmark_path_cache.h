// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the bookmark top most row that the user was last
// viewing.
@interface BookmarkPathCache : NSObject

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Caches the bookmark top most row that the user was last viewing.
+ (void)cacheBookmarkTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                    topMostRow:(int)topMostRow;

// Gets the bookmark top most row that the user was last viewing. Returns YES if
// a valid cache exists. `folderId` and `topMostRow` are out variables, only
// populated if the return is YES.
+ (BOOL)getBookmarkTopMostRowCacheWithPrefService:(PrefService*)prefService
                                            model:
                                                (bookmarks::BookmarkModel*)model
                                         folderId:(int64_t*)folderId
                                       topMostRow:(int*)topMostRow;

// Clears the bookmark top most row cache.
+ (void)clearBookmarkTopMostRowCacheWithPrefService:(PrefService*)prefService;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_
