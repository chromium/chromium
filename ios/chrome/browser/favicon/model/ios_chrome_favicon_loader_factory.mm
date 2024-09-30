// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildFaviconLoader(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<FaviconLoader>(
      IOSChromeLargeIconServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
FaviconLoader* IOSChromeFaviconLoaderFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
FaviconLoader* IOSChromeFaviconLoaderFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<FaviconLoader*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
FaviconLoader* IOSChromeFaviconLoaderFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<FaviconLoader*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
    : BrowserStateKeyedServiceFactory(
          "FaviconLoader",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
}

IOSChromeFaviconLoaderFactory::~IOSChromeFaviconLoaderFactory() {}

std::unique_ptr<KeyedService>
IOSChromeFaviconLoaderFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildFaviconLoader(context);
}

web::BrowserState* IOSChromeFaviconLoaderFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromeFaviconLoaderFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
