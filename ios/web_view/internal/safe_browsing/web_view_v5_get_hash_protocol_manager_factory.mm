// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_v5_get_hash_protocol_manager_factory.h"

#import <memory>
#import <string>
#import <string_view>

#import "base/no_destructor.h"
#import "build/branding_buildflags.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/safe_browsing/web_view_v5_search_hashes_cache_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr std::string_view kClientName = "googlechrome";
#else
inline constexpr std::string_view kClientName = "chromium";
#endif

}  // namespace

namespace ios_web_view {

// static
safe_browsing::V5GetHashProtocolManager*
WebViewV5GetHashProtocolManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<safe_browsing::V5GetHashProtocolManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
WebViewV5GetHashProtocolManagerFactory*
WebViewV5GetHashProtocolManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewV5GetHashProtocolManagerFactory> instance;
  return instance.get();
}

WebViewV5GetHashProtocolManagerFactory::WebViewV5GetHashProtocolManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "WebViewV5GetHashProtocolManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewV5SearchHashesCacheFactory::GetInstance());
}

std::unique_ptr<KeyedService>
WebViewV5GetHashProtocolManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  SafeBrowsingService* safe_browsing_service =
      ApplicationContext::GetInstance()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<safe_browsing::V5GetHashProtocolManager>(
      safe_browsing_service->GetURLLoaderFactory(),
      safe_browsing::GetV4ProtocolConfig(
          /*client_name=*/std::string(kClientName),
          /*disable_auto_update=*/false),
      WebViewV5SearchHashesCacheFactory::GetForBrowserState(browser_state));
}

web::BrowserState* WebViewV5GetHashProtocolManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}

}  // namespace ios_web_view
