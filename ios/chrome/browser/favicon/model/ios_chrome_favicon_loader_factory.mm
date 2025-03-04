// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/favicon/model/favicon_loader_impl.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildFaviconLoader(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<FaviconLoaderImpl>(
      IOSChromeLargeIconServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
FaviconLoader* IOSChromeFaviconLoaderFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<FaviconLoader>(profile,
                                                              /*create=*/false);
}

// static
FaviconLoader* IOSChromeFaviconLoaderFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<FaviconLoader>(profile,
                                                              /*create=*/true);
}

// static
IOSChromeFaviconLoaderFactory* IOSChromeFaviconLoaderFactory::GetInstance() {
  static base::NoDestructor<IOSChromeFaviconLoaderFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSChromeFaviconLoaderFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildFaviconLoader);
}

IOSChromeFaviconLoaderFactory::IOSChromeFaviconLoaderFactory()
    : ProfileKeyedServiceFactoryIOS("FaviconLoader",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
}

IOSChromeFaviconLoaderFactory::~IOSChromeFaviconLoaderFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeFaviconLoaderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildFaviconLoader(context);
}
