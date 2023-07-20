// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;

@class SpotlightInterface;
@class SearchableItemFactory;

// This spotlight manager handles indexing of sites shown on the NTP. Because of
// privacy concerns, only sites shown on the NTP are indexed; therefore, this
// manager mirrors the functionality seen in google_landing_view_controller. It
// uses suggestions (most likely) as a data source if the user is logged in and
// top sites otherwise.

@interface TopSitesSpotlightManager : NSObject

+ (TopSitesSpotlightManager*)topSitesSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

- (instancetype)init NS_UNAVAILABLE;

/// Facade interface for the spotlight API.
@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

/// A searchable item factory to create searchable items.
@property(nonatomic, readonly) SearchableItemFactory* searchableItemFactory;

// Reindexes all top sites, batching reindexes by 1 second.
- (void)reindexTopSites;

// Called before the instance is deallocated. This method should be overridden
// by the subclasses and de-activate the instance.
- (void)shutdown;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_
