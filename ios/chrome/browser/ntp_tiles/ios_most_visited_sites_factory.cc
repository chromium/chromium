// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"

#include "base/task/post_task.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/icon_cacher_impl.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_popular_sites_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

std::unique_ptr<ntp_tiles::MostVisitedSites>
IOSMostVisitedSitesFactory::NewForBrowserState(
    ChromeBrowserState* browser_state) {
  return std::make_unique<ntp_tiles::MostVisitedSites>(
      browser_state->GetPrefs(),
      ios::TopSitesFactory::GetForBrowserState(browser_state),
      suggestions::SuggestionsServiceFactory::GetForBrowserState(browser_state),
      IOSPopularSitesFactory::NewForBrowserState(browser_state),
      /*custom_links=*/nullptr,
      std::make_unique<ntp_tiles::IconCacherImpl>(
          ios::FaviconServiceFactory::GetForBrowserState(
              browser_state, ServiceAccessType::IMPLICIT_ACCESS),
          IOSChromeLargeIconServiceFactory::GetForBrowserState(browser_state),
          std::make_unique<image_fetcher::ImageFetcherImpl>(
              image_fetcher::CreateIOSImageDecoder(),
              browser_state->GetSharedURLLoaderFactory())),
      nil);
}
