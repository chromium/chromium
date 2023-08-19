// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

// static
safe_browsing::TailoredSecurityService*
TailoredSecurityServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<safe_browsing::TailoredSecurityService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
TailoredSecurityServiceFactory* TailoredSecurityServiceFactory::GetInstance() {
  static base::NoDestructor<TailoredSecurityServiceFactory> instance;
  return instance.get();
}

TailoredSecurityServiceFactory::TailoredSecurityServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TailoredSecurityService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TailoredSecurityServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  return std::make_unique<safe_browsing::ChromeTailoredSecurityService>(
      chrome_browser_state,
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state),
      SyncServiceFactory::GetForBrowserState(chrome_browser_state));
}
