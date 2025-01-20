// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client_factory.h"

#import <memory>

#import "base/check.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
SafeBrowsingClient* WebViewSafeBrowsingClientFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<SafeBrowsingClient*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
WebViewSafeBrowsingClientFactory*
WebViewSafeBrowsingClientFactory::GetInstance() {
  static base::NoDestructor<WebViewSafeBrowsingClientFactory> instance;
  return instance.get();
}

WebViewSafeBrowsingClientFactory::WebViewSafeBrowsingClientFactory()
    : BrowserStateKeyedServiceFactory(
          "WebViewSafeBrowsingClient",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewSafeBrowsingClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<WebViewSafeBrowsingClient>(browser_state->GetPrefs());
}

web::BrowserState* WebViewSafeBrowsingClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}

}  // namespace ios_web_view
