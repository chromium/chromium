// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/kids_chrome_management_client_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/supervised_user/core/browser/kids_chrome_management_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
KidsChromeManagementClient*
KidsChromeManagementClientFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<KidsChromeManagementClient*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
KidsChromeManagementClientFactory*
KidsChromeManagementClientFactory::GetInstance() {
  static base::NoDestructor<KidsChromeManagementClientFactory> instance;
  return instance.get();
}

KidsChromeManagementClientFactory::KidsChromeManagementClientFactory()
    : BrowserStateKeyedServiceFactory(
          "KidsChromeManagementClient",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
KidsChromeManagementClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<KidsChromeManagementClient>(
      context->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(context)));
}

web::BrowserState* KidsChromeManagementClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
