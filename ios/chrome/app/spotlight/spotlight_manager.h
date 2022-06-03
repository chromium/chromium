// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;

// Allows Chrome to add the bookmarks of |bookmarkModel| in the systemwide
// Spotlight search index.
// Bookmarks are added, removed or updated in Spotlight based on
// BookmarkModelObserver notification.
// As there is no possibility to check the state of the index, a global
// reindexing is triggered on cold start every 7 days to enforce
// bookmarks/spotlight synchronization. A global reindexing will clear the index
// and reindex the first 1000 bookmarks.
@interface SpotlightManager : NSObject

// Creates a SpotlightManager tracking and indexing various browser state
// elements such as most actives and bookmarks.
// |browserState| must not be nil.
// There should be only one SpotlightManager observing |browserState|.
+ (SpotlightManager*)spotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

// Resyncs the index if necessary
- (void)resyncIndex;

// Called before the instance is deallocated. This method should be overridden
// by the subclasses and de-activate the instance.
- (void)shutdown;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_SPOTLIGHT_MANAGER_H_
