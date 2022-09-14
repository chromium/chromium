// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace web {
class BrowserState;
}  // namespace web

class SafeBrowsingClient;

namespace ios_web_view {

// Singleton that owns all SafeBrowsingClients and associates them with
// a browser state.
class WebViewSafeBrowsingClientFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static SafeBrowsingClient* GetForBrowserState(
      web::BrowserState* browser_state);
  static WebViewSafeBrowsingClientFactory* GetInstance();

  WebViewSafeBrowsingClientFactory(const WebViewSafeBrowsingClientFactory&) =
      delete;
  WebViewSafeBrowsingClientFactory& operator=(
      const WebViewSafeBrowsingClientFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewSafeBrowsingClientFactory>;

  WebViewSafeBrowsingClientFactory();
  ~WebViewSafeBrowsingClientFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_SAFE_BROWSING_CLIENT_FACTORY_H_
