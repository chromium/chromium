// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const int kDipForServerRequests = 32;
const favicon_base::IconType kIconTypeForServerRequests =
    favicon_base::IconType::kTouchIcon;
const char kGoogleServerClientParam[] = "chrome";

std::unique_ptr<KeyedService> BuildLargeIconService(ProfileIOS* profile) {
  return std::make_unique<favicon::LargeIconServiceImpl>(
      ios::FaviconServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          profile->GetSharedURLLoaderFactory()),
      kDipForServerRequests, kIconTypeForServerRequests,
      kGoogleServerClientParam);
}

}  // namespace

// static
IOSChromeLargeIconServiceFactory::TestingFactory
IOSChromeLargeIconServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildLargeIconService);
}

IOSChromeLargeIconServiceFactory::IOSChromeLargeIconServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "LargeIconService",
          ProfileSelection::kOwnInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::FaviconServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeLargeIconServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildLargeIconService(profile);
}
