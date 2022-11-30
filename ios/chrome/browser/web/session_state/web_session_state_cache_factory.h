// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
@class WebSessionStateCache;

// Singleton that owns all WebSessionStateCaches and associates them with
// ChromeBrowserState.
class WebSessionStateCacheFactory : public BrowserStateKeyedServiceFactory {
 public:
  static WebSessionStateCache* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static WebSessionStateCacheFactory* GetInstance();

  WebSessionStateCacheFactory(const WebSessionStateCacheFactory&) = delete;
  WebSessionStateCacheFactory& operator=(const WebSessionStateCacheFactory&) =
      delete;

 private:
  friend class base::NoDestructor<WebSessionStateCacheFactory>;

  WebSessionStateCacheFactory();
  ~WebSessionStateCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_CACHE_FACTORY_H_
