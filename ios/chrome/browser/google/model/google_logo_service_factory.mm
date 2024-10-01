// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google/model/google_logo_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
GoogleLogoService* GoogleLogoServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
GoogleLogoService* GoogleLogoServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<GoogleLogoService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
GoogleLogoServiceFactory* GoogleLogoServiceFactory::GetInstance() {
  static base::NoDestructor<GoogleLogoServiceFactory> instance;
  return instance.get();
}

GoogleLogoServiceFactory::GoogleLogoServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GoogleLogoService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

GoogleLogoServiceFactory::~GoogleLogoServiceFactory() {}

std::unique_ptr<KeyedService> GoogleLogoServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<GoogleLogoService>(
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}

web::BrowserState* GoogleLogoServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
