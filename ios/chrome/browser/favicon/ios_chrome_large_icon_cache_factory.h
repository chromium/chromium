// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;
class LargeIconCache;

// Singleton that owns all LargeIconCaches and associates them with
// ChromeBrowserState.
class IOSChromeLargeIconCacheFactory : public BrowserStateKeyedServiceFactory {
 public:
  static LargeIconCache* GetForBrowserState(ChromeBrowserState* browser_state);

  static IOSChromeLargeIconCacheFactory* GetInstance();

  IOSChromeLargeIconCacheFactory(const IOSChromeLargeIconCacheFactory&) =
      delete;
  IOSChromeLargeIconCacheFactory& operator=(
      const IOSChromeLargeIconCacheFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeLargeIconCacheFactory>;

  IOSChromeLargeIconCacheFactory();
  ~IOSChromeLargeIconCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
