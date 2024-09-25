// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_tiles/model/ios_most_visited_sites_factory.h"

#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/icon_cacher_impl.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/model/top_sites_factory.h"
#include "ios/chrome/browser/ntp_tiles/model/ios_popular_sites_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

std::unique_ptr<ntp_tiles::MostVisitedSites>
IOSMostVisitedSitesFactory::NewForBrowserState(ProfileIOS* profile) {
  return std::make_unique<ntp_tiles::MostVisitedSites>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      SupervisedUserServiceFactory::GetForProfile(profile),
      ios::TopSitesFactory::GetForProfile(profile),
      IOSPopularSitesFactory::NewForBrowserState(profile),
      /*custom_links=*/nullptr,
      std::make_unique<ntp_tiles::IconCacherImpl>(
          ios::FaviconServiceFactory::GetForBrowserState(
              profile, ServiceAccessType::IMPLICIT_ACCESS),
          IOSChromeLargeIconServiceFactory::GetForProfile(profile),
          std::make_unique<image_fetcher::ImageFetcherImpl>(
              image_fetcher::CreateIOSImageDecoder(),
              profile->GetSharedURLLoaderFactory()),
          /*data_decoder=*/nullptr),
      false);
}
