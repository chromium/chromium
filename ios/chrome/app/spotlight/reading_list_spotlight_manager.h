// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

class ChromeBrowserState;
class ReadingListModel;
@class CSSearchableItem;
@class SpotlightInterface;

/// Manages Reading List items in Spotlight search.
@interface ReadingListSpotlightManager : BaseSpotlightManager

/// Model observed by this instance.
@property(nonatomic, assign, readonly) ReadingListModel* model;

/// Convenience initializer with browser state.
/// Returns a new instance of ReadingListSpotlightManager and retrieves all
/// dependencies from `browserState`.
+ (ReadingListSpotlightManager*)readingListSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                        readingListModel:(ReadingListModel*)readingListModel
                      spotlightInterface:(SpotlightInterface*)spotlightInterface
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain
                      spotlightInterface:(SpotlightInterface*)spotlightInterface
    NS_UNAVAILABLE;

/// Immediately clears and reindexes the reading list items in Spotlight. Calls
/// `completionHandler` when done.
- (void)clearAndReindexReadingListWithCompletionBlock:
    (void (^)(NSError* error))completionHandler;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_READING_LIST_SPOTLIGHT_MANAGER_H_
