// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace safe_browsing {
class V5GetHashProtocolManager;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns V5GetHashProtocolManager objects, one for each active
// BrowserState.
class WebViewV5GetHashProtocolManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the V5GetHashProtocolManager associated with `browser_state`.
  static safe_browsing::V5GetHashProtocolManager* GetForBrowserState(
      WebViewBrowserState* browser_state);

  // Returns the singleton instance of WebViewV5GetHashProtocolManagerFactory.
  static WebViewV5GetHashProtocolManagerFactory* GetInstance();

  WebViewV5GetHashProtocolManagerFactory(
      const WebViewV5GetHashProtocolManagerFactory&) = delete;
  WebViewV5GetHashProtocolManagerFactory& operator=(
      const WebViewV5GetHashProtocolManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<WebViewV5GetHashProtocolManagerFactory>;

  WebViewV5GetHashProtocolManagerFactory();
  ~WebViewV5GetHashProtocolManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SAFE_BROWSING_WEB_VIEW_V5_GET_HASH_PROTOCOL_MANAGER_FACTORY_H_
