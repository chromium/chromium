// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/shopping_service_factory.h"

#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "components/commerce/core/shopping_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/session_proto_db_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace commerce {

// static
ShoppingServiceFactory* ShoppingServiceFactory::GetInstance() {
  static base::NoDestructor<ShoppingServiceFactory> instance;
  return instance.get();
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserState(
    web::BrowserState* state) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

// static
ShoppingService* ShoppingServiceFactory::GetForBrowserStateIfExists(
    web::BrowserState* state) {
  return static_cast<ShoppingService*>(
      GetInstance()->GetServiceForBrowserState(state, false));
}

ShoppingServiceFactory::ShoppingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ShoppingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SessionProtoDBFactory<
            commerce_subscription_db::CommerceSubscriptionContentProto>::
                GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> ShoppingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ChromeBrowserState* chrome_state =
      ChromeBrowserState::FromBrowserState(state);
  PrefService* pref_service = chrome_state ? chrome_state->GetPrefs() : nullptr;
  return std::make_unique<ShoppingService>(
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      GetApplicationContext()->GetApplicationLocale(),
      ios::BookmarkModelFactory::GetInstance()->GetForBrowserState(
          chrome_state),
      OptimizationGuideServiceFactory::GetForBrowserState(chrome_state),
      pref_service, IdentityManagerFactory::GetForBrowserState(chrome_state),
      chrome_state->GetSharedURLLoaderFactory(),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(chrome_state),
      PowerBookmarkServiceFactory::GetForBrowserState(chrome_state));
}

bool ShoppingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace commerce
