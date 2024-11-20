// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_helper_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewSafeBrowsingHelperFactory*
WebViewSafeBrowsingHelperFactory::GetInstance() {
  static base::NoDestructor<WebViewSafeBrowsingHelperFactory> instance;
  return instance.get();
}

// static
SafeBrowsingHelper* WebViewSafeBrowsingHelperFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<SafeBrowsingHelper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

WebViewSafeBrowsingHelperFactory::WebViewSafeBrowsingHelperFactory()
    : BrowserStateKeyedServiceFactory(
          "WebViewSafeBrowsingHelper",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewSafeBrowsingHelperFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  SafeBrowsingService* safe_browsing_service =
      ApplicationContext::GetInstance()->GetSafeBrowsingService();
  return std::make_unique<SafeBrowsingHelper>(browser_state->GetPrefs(),
                                              safe_browsing_service, nullptr);
}

bool WebViewSafeBrowsingHelperFactory::ServiceIsCreatedWithBrowserState()
    const {
  return true;
}

}  // namespace ios_web_view
