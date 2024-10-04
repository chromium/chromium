// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// This spotlight manager handles indexing of sites shown on the NTP. Because of
// privacy concerns, only sites shown on the NTP are indexed; therefore, this
// manager mirrors the functionality seen in google_landing_view_controller. It
// uses suggestions (most likely) as a data source if the user is logged in and
// top sites otherwise.

@interface TopSitesSpotlightManager : BaseSpotlightManager

+ (TopSitesSpotlightManager*)topSitesSpotlightManagerWithProfile:
    (ProfileIOS*)profile;

- (instancetype)init NS_UNAVAILABLE;

// Reindexes all top sites, batching reindexes by 1 second.
- (void)reindexTopSites;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_TOPSITES_SPOTLIGHT_MANAGER_H_
