// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_factory.h"

#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {
namespace {

std::unique_ptr<KeyedService> BuildPlaceholderService(ProfileIOS* profile) {
  return std::make_unique<PlaceholderService>(
      IOSChromeFaviconLoaderFactory::GetForProfile(profile),
      ios::TemplateURLServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
PlaceholderService* PlaceholderServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PlaceholderService>(
      profile, /*create=*/true);
}

// static
PlaceholderServiceFactory* PlaceholderServiceFactory::GetInstance() {
  static base::NoDestructor<PlaceholderServiceFactory> instance;
  return instance.get();
}

// static
PlaceholderServiceFactory::TestingFactory
PlaceholderServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildPlaceholderService);
}

PlaceholderServiceFactory::PlaceholderServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PlaceholderService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IOSChromeFaviconLoaderFactory::GetInstance());
}

PlaceholderServiceFactory::~PlaceholderServiceFactory() {}

std::unique_ptr<KeyedService>
PlaceholderServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return BuildPlaceholderService(profile);
}

}  // namespace ios
