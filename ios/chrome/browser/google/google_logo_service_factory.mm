// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google/google_logo_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/google/google_logo_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
GoogleLogoService* GoogleLogoServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<GoogleLogoService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<GoogleLogoService>(
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state),
      IdentityManagerFactory::GetForBrowserState(browser_state),
      browser_state->GetSharedURLLoaderFactory());
}

web::BrowserState* GoogleLogoServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
