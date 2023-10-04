// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace {

bool IsEnhancedProtectionEnabled(ChromeBrowserState* browser_state) {
  return safe_browsing::IsEnhancedProtectionEnabled(
      *(browser_state->GetPrefs()));
}

network::mojom::NetworkContext* GetNetworkContext() {
  return GetApplicationContext()->GetSafeBrowsingService()->GetNetworkContext();
}

}  // namespace

// static
safe_browsing::HashRealTimeService*
HashRealTimeServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<safe_browsing::HashRealTimeService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
HashRealTimeServiceFactory* HashRealTimeServiceFactory::GetInstance() {
  static base::NoDestructor<HashRealTimeServiceFactory> instance;
  return instance.get();
}

HashRealTimeServiceFactory::HashRealTimeServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "HashRealTimeService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(OhttpKeyServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
HashRealTimeServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          safe_browsing_service->GetURLLoaderFactory());
  return std::make_unique<safe_browsing::HashRealTimeService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      base::BindRepeating(&GetNetworkContext),
      VerdictCacheManagerFactory::GetForBrowserState(chrome_browser_state),
      OhttpKeyServiceFactory::GetForBrowserState(chrome_browser_state),
      base::BindRepeating(&IsEnhancedProtectionEnabled, chrome_browser_state),
      /*webui_delegate=*/nullptr);
}
