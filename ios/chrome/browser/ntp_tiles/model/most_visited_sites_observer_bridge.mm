// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/most_visited_sites_observer_bridge.h"

#import "base/check.h"

namespace ntp_tiles {

MostVisitedSitesObserverBridge::MostVisitedSitesObserverBridge(
    id<MostVisitedSitesObserving> observer,
    MostVisitedSites* most_visited_sites)
    : observer_(observer), most_visited_sites_(most_visited_sites) {
  CHECK(observer_);
  CHECK(most_visited_sites_);
}

MostVisitedSitesObserverBridge::~MostVisitedSitesObserverBridge() {}

void MostVisitedSitesObserverBridge::OnURLsAvailable(
    bool is_user_triggered,
    const std::map<SectionType, NTPTilesVector>& sections) {
  const NTPTilesVector& tiles = sections.at(SectionType::PERSONALIZED);
  [observer_ mostVisitedSites:most_visited_sites_ didUpdateTiles:tiles];
}

void MostVisitedSitesObserverBridge::OnIconMadeAvailable(const GURL& site_url) {
  [observer_ mostVisitedSites:most_visited_sites_
       didUpdateFaviconForURL:site_url];
}

}  // namespace ntp_tiles
