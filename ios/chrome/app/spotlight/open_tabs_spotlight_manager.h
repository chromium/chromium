// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

namespace favicon {
class LargeIconService;
}

class BrowserList;
class ChromeBrowserState;
@class CSSearchableItem;
@class SpotlightInterface;
@class SearchableItemFactory;

/// Manages Open Tab items in Spotlight search.
@interface OpenTabsSpotlightManager : NSObject

- (instancetype)init NS_UNAVAILABLE;

/// Model observed by this instance.
@property(nonatomic, assign, readonly) BrowserList* browserList;

/// Spotlight API endpoint.
@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

/// A searchable item factory to create searchable items.
@property(nonatomic, readonly) SearchableItemFactory* searchableItemFactory;

/// Convenience initializer with browser state.
/// Returns a new instance of OpenTabsSpotlightManager and retrieves all
/// dependencies from `browserState`.
+ (OpenTabsSpotlightManager*)openTabsSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                 browserList:(BrowserList*)browserList
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_DESIGNATED_INITIALIZER;

/// Immediately clears and reindexes the Open Tab items in Spotlight.
- (void)clearAndReindexOpenTabs;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
