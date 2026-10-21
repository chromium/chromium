// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"

#import <utility>

#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/favicon/model/favicon_loader_impl.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildFaviconLoader(ProfileIOS* profile) {
  return std::make_unique<FaviconLoaderImpl>(
      IOSChromeLargeIconServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
IOSChromeFaviconLoaderFactory::TestingFactory
IOSChromeFaviconLoaderFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildFaviconLoader);
}

IOSChromeFaviconLoaderFactory::IOSChromeFaviconLoaderFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "FaviconLoader",
          ProfileSelection::kRedirectedInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeFaviconLoaderFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildFaviconLoader(profile);
}
