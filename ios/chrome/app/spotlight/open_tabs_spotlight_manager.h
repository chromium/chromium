// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

class BrowserList;
class ChromeBrowserState;
@class CSSearchableItem;
@class SpotlightInterface;

/// Manages Open Tab items in Spotlight search.
@interface OpenTabsSpotlightManager : BaseSpotlightManager

/// Model observed by this instance.
@property(nonatomic, assign, readonly) BrowserList* browserList;

/// Convenience initializer with browser state.
/// Returns a new instance of OpenTabsSpotlightManager and retrieves all
/// dependencies from `browserState`.
+ (OpenTabsSpotlightManager*)openTabsSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                             browserList:(BrowserList*)browserList
                      spotlightInterface:(SpotlightInterface*)spotlightInterface
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain
                      spotlightInterface:(SpotlightInterface*)spotlightInterface
    NS_UNAVAILABLE;

/// Immediately clears and reindexes the Open Tab items in Spotlight.
- (void)clearAndReindexOpenTabs;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_OPEN_TABS_SPOTLIGHT_MANAGER_H_
