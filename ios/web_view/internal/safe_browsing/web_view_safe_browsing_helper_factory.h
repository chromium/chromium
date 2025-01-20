// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_HELPER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_HELPER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class SafeBrowsingHelper;

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all SafeBrowsingHelpers and associate them with a
// BrowserState.
class WebViewSafeBrowsingHelperFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static WebViewSafeBrowsingHelperFactory* GetInstance();
  static SafeBrowsingHelper* GetForBrowserState(
      WebViewBrowserState* browser_state);

 private:
  friend class base::NoDestructor<WebViewSafeBrowsingHelperFactory>;

  WebViewSafeBrowsingHelperFactory();
  ~WebViewSafeBrowsingHelperFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_HELPER_FACTORY_H_
