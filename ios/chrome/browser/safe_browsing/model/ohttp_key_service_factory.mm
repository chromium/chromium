// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/ohttp_key_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

// static
safe_browsing::OhttpKeyService* OhttpKeyServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<safe_browsing::OhttpKeyService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
OhttpKeyServiceFactory* OhttpKeyServiceFactory::GetInstance() {
  static base::NoDestructor<OhttpKeyServiceFactory> instance;
  return instance.get();
}

OhttpKeyServiceFactory::OhttpKeyServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "OhttpKeyService",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService> OhttpKeyServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  if (!base::FeatureList::IsEnabled(safe_browsing::kHashRealTimeOverOhttp) &&
      !safe_browsing::hash_realtime_utils::
          IsHashRealTimeLookupEligibleInSessionAndLocation(
              safe_browsing::hash_realtime_utils::GetCountryCode(
                  GetApplicationContext()->GetVariationsService()))) {
    return nullptr;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          safe_browsing_service->GetURLLoaderFactory());
  return std::make_unique<safe_browsing::OhttpKeyService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      chrome_browser_state->GetPrefs());
}

bool OhttpKeyServiceFactory::ServiceIsCreatedWithBrowserState() const {
  // The service is created early to start async key fetch.
  return true;
}
