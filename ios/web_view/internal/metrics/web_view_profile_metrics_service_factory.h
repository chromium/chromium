// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_METRICS_WEB_VIEW_PROFILE_METRICS_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_METRICS_WEB_VIEW_PROFILE_METRICS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace metrics {
class ProfileMetricsService;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all ProfileMetricsServices and associates them with
// a browser state.
class WebViewProfileMetricsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static metrics::ProfileMetricsService* GetForBrowserState(
      ios_web_view::WebViewBrowserState* browser_state);
  static WebViewProfileMetricsServiceFactory* GetInstance();

  WebViewProfileMetricsServiceFactory(
      const WebViewProfileMetricsServiceFactory&) = delete;
  WebViewProfileMetricsServiceFactory& operator=(
      const WebViewProfileMetricsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewProfileMetricsServiceFactory>;

  WebViewProfileMetricsServiceFactory();
  ~WebViewProfileMetricsServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_METRICS_WEB_VIEW_PROFILE_METRICS_SERVICE_FACTORY_H_
