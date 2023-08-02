// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client_factory.h"

#import <memory>

#import "base/base_paths.h"
#import "base/check.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web/public/browser_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
SafeBrowsingClient* WebViewSafeBrowsingClientFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
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
  std::unique_ptr<WebViewSafeBrowsingClient> service =
      std::make_unique<WebViewSafeBrowsingClient>();

  // Ensure that Safe Browsing is initialized.
  SafeBrowsingService* safe_browsing_service =
      ApplicationContext::GetInstance()->GetSafeBrowsingService();
  base::FilePath data_path;
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_path));
  safe_browsing_service->Initialize(
      browser_state->GetRecordingBrowserState()->GetPrefs(), data_path,
      /*safe_browsing_metrics_collector=*/nullptr);
  return service;
}

web::BrowserState* WebViewSafeBrowsingClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return context;
}

}  // namespace ios_web_view
