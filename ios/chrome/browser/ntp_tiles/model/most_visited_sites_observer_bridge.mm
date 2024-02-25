// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/most_visited_sites_observer_bridge.h"

namespace ntp_tiles {

MostVisitedSitesObserverBridge::MostVisitedSitesObserverBridge(
    id<MostVisitedSitesObserving> observer) {
  observer_ = observer;
}

MostVisitedSitesObserverBridge::~MostVisitedSitesObserverBridge() {}

void MostVisitedSitesObserverBridge::OnURLsAvailable(
    const std::map<SectionType, NTPTilesVector>& sections) {
  const NTPTilesVector& most_visited = sections.at(SectionType::PERSONALIZED);
  [observer_ onMostVisitedURLsAvailable:most_visited];
}

void MostVisitedSitesObserverBridge::OnIconMadeAvailable(const GURL& site_url) {
  [observer_ onIconMadeAvailable:site_url];
}

}  // namespace ntp_tiles
