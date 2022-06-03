// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_TILES_IOS_MOST_VISITED_SITES_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_TILES_IOS_MOST_VISITED_SITES_FACTORY_H_

#include <memory>

class ChromeBrowserState;

namespace ntp_tiles {
class MostVisitedSites;
}

class IOSMostVisitedSitesFactory {
 public:
  static std::unique_ptr<ntp_tiles::MostVisitedSites> NewForBrowserState(
      ChromeBrowserState* browser_state);
};

#endif  // IOS_CHROME_BROWSER_NTP_TILES_IOS_MOST_VISITED_SITES_FACTORY_H_
