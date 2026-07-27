// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_v5_search_hashes_cache_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/safe_browsing/core/browser/db/v5_search_hashes_cache.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
safe_browsing::V5SearchHashesCache*
WebViewV5SearchHashesCacheFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<safe_browsing::V5SearchHashesCache*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
WebViewV5SearchHashesCacheFactory*
WebViewV5SearchHashesCacheFactory::GetInstance() {
  static base::NoDestructor<WebViewV5SearchHashesCacheFactory> instance;
  return instance.get();
}

WebViewV5SearchHashesCacheFactory::WebViewV5SearchHashesCacheFactory()
    : BrowserStateKeyedServiceFactory(
          "WebViewV5SearchHashesCache",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewV5SearchHashesCacheFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<safe_browsing::V5SearchHashesCache>(
      /*history_service=*/nullptr);
}

web::BrowserState* WebViewV5SearchHashesCacheFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}

}  // namespace ios_web_view
