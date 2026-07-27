// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_SEARCH_HASHES_CACHE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_SEARCH_HASHES_CACHE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace safe_browsing {
class V5SearchHashesCache;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns V5SearchHashesCache objects, one for each active
// BrowserState.
class WebViewV5SearchHashesCacheFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the V5SearchHashesCache associated with `browser_state`.
  static safe_browsing::V5SearchHashesCache* GetForBrowserState(
      WebViewBrowserState* browser_state);

  // Returns the singleton instance of WebViewV5SearchHashesCacheFactory.
  static WebViewV5SearchHashesCacheFactory* GetInstance();

  WebViewV5SearchHashesCacheFactory(const WebViewV5SearchHashesCacheFactory&) =
      delete;
  WebViewV5SearchHashesCacheFactory& operator=(
      const WebViewV5SearchHashesCacheFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewV5SearchHashesCacheFactory>;

  WebViewV5SearchHashesCacheFactory();
  ~WebViewV5SearchHashesCacheFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_SEARCH_HASHES_CACHE_FACTORY_H_
