// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_TILES_MODEL_MOST_VISITED_SITES_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_NTP_TILES_MODEL_MOST_VISITED_SITES_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <map>

#include "components/ntp_tiles/most_visited_sites.h"

// Observes MostVisitedSites events from Objective-C. To use as a
// ntp_tiles::MostVisitedSites::Observer, wrap in a
// MostVisitedSitesObserverBridge.
@protocol MostVisitedSitesObserving<NSObject>

// Invoked by ntp_tiles::MostVisitedSites::Observer::OnMostVisitedURLsAvailable.
- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited;

// Invoked by ntp_tiles::MostVisitedSites::Observer::OnIconMadeAvailable.
- (void)onIconMadeAvailable:(const GURL&)siteURL;

@end

namespace ntp_tiles {

// Observer for the MostVisitedSites that translates all the callbacks to
// Objective-C calls.
class MostVisitedSitesObserverBridge : public MostVisitedSites::Observer {
 public:
  MostVisitedSitesObserverBridge(id<MostVisitedSitesObserving> observer);

  MostVisitedSitesObserverBridge(const MostVisitedSitesObserverBridge&) =
      delete;
  MostVisitedSitesObserverBridge& operator=(
      const MostVisitedSitesObserverBridge&) = delete;

  ~MostVisitedSitesObserverBridge() override;

  void OnURLsAvailable(
      const std::map<SectionType, NTPTilesVector>& sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

 private:
  __weak id<MostVisitedSitesObserving> observer_ = nil;
};

}  // namespace ntp_tiles

#endif  // IOS_CHROME_BROWSER_NTP_TILES_MODEL_MOST_VISITED_SITES_OBSERVER_BRIDGE_H_
