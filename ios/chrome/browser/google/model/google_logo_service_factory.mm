// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google/model/google_logo_service_factory.h"

#import <utility>

#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

GoogleLogoServiceFactory::GoogleLogoServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "GoogleLogoService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService> GoogleLogoServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<GoogleLogoService>(
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}
