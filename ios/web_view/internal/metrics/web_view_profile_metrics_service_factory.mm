// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/metrics/web_view_profile_metrics_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/metrics/profile_metrics_service.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
metrics::ProfileMetricsService*
WebViewProfileMetricsServiceFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<metrics::ProfileMetricsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewProfileMetricsServiceFactory*
WebViewProfileMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewProfileMetricsServiceFactory> instance;
  return instance.get();
}

WebViewProfileMetricsServiceFactory::WebViewProfileMetricsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProfileMetricsService",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewProfileMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<metrics::ProfileMetricsService>(
      metrics::ProfileMetricsContext());
}

}  // namespace ios_web_view
