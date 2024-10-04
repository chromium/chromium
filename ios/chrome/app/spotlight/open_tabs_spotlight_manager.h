// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace favicon {
class LargeIconService;
}

class BrowserList;
@class CSSearchableItem;

/// Manages Open Tab items in Spotlight search.
@interface OpenTabsSpotlightManager : BaseSpotlightManager

/// Model observed by this instance.
@property(nonatomic, assign, readonly) BrowserList* browserList;

/// Convenience initializer with profile.
/// Returns a new instance of OpenTabsSpotlightManager and retrieves all
/// dependencies from `profile`.
+ (OpenTabsSpotlightManager*)openTabsSpotlightManagerWithProfile:
    (ProfileIOS*)profile;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                 browserList:(BrowserList*)browserList
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_UNAVAILABLE;

/// Immediately clears and reindexes the Open Tab items in Spotlight.
- (void)clearAndReindexOpenTabs;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
