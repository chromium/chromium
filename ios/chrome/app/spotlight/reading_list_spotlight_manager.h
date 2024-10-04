// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace favicon {
class LargeIconService;
}

class ReadingListModel;
@class CSSearchableItem;

/// Manages Reading List items in Spotlight search.
@interface ReadingListSpotlightManager : BaseSpotlightManager

/// Model observed by this instance.
@property(nonatomic, assign, readonly) ReadingListModel* model;

/// Convenience initializer with profile.
/// Returns a new instance of ReadingListSpotlightManager and retrieves all
/// dependencies from `profile`.
+ (ReadingListSpotlightManager*)readingListSpotlightManagerWithProfile:
    (ProfileIOS*)profile;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
            readingListModel:(ReadingListModel*)readingListModel
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_UNAVAILABLE;

/// Immediately clears and reindexes the reading list items in Spotlight. Calls
/// `completionHandler` when done.
- (void)clearAndReindexReadingList;

// Indexes all existing reading list items in spotlight.
- (void)indexAllReadingListItems;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
