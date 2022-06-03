// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}

@class CSSearchableItem;
@class TopSitesSpotlightManager;

@protocol BookmarkUpdatedDelegate

// Called when a bookmark is updated.
- (void)bookmarkUpdated;

@end

@interface BookmarksSpotlightManager : BaseSpotlightManager

// The delegate notified when a bookmark is updated.
@property(nonatomic, weak) id<BookmarkUpdatedDelegate> delegate;

+ (BookmarksSpotlightManager*)bookmarksSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

// Checks the date of the latest global indexation and reindex all bookmarks if
// needed.
- (void)reindexBookmarksIfNeeded;

// Methods below here are for testing use only.

- (instancetype)
initWithLargeIconService:(favicon::LargeIconService*)largeIconService
           bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel;

// Recursively adds node ancestors titles to keywords. Permanent nodes are
// ignored.
- (void)getParentKeywordsForNode:(const bookmarks::BookmarkNode*)node
                         inArray:(NSMutableArray*)keywords;

// Adds keywords to |item|.
- (void)addKeywords:(NSArray*)keywords toSearchableItem:(CSSearchableItem*)item;

// Called before the instance is deallocated. This method should be overridden
// by the subclasses and de-activate the instance.
- (void)shutdown;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_
