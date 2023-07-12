// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

namespace favicon {
class LargeIconService;
}

class ChromeBrowserState;
class ReadingListModel;
@class CSSearchableItem;
@class SpotlightInterface;
@class SearchableItemFactory;

/// Manages Reading List items in Spotlight search.
@interface ReadingListSpotlightManager : NSObject

- (instancetype)init NS_UNAVAILABLE;

/// Facade interface for the spotlight API.
@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

/// A searchable item factory to create searchable items.
@property(nonatomic, readonly) SearchableItemFactory* searchableItemFactory;

/// Model observed by this instance.
@property(nonatomic, assign, readonly) ReadingListModel* model;

/// Convenience initializer with browser state.
/// Returns a new instance of ReadingListSpotlightManager and retrieves all
/// dependencies from `browserState`.
+ (ReadingListSpotlightManager*)readingListSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
            readingListModel:(ReadingListModel*)readingListModel
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_DESIGNATED_INITIALIZER;

/// Immediately clears and reindexes the reading list items in Spotlight. Calls
/// `completionHandler` when done.
- (void)clearAndReindexReadingList;

// Indexes all existing reading list items in spotlight.
- (void)indexAllReadingListItems;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
