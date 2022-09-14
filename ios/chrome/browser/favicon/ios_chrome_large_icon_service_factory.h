// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;

namespace favicon {
class LargeIconService;
}

// Singleton that owns all LargeIconService and associates them with
// ChromeBrowserState.
class IOSChromeLargeIconServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static favicon::LargeIconService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static IOSChromeLargeIconServiceFactory* GetInstance();

  // Returns the default factory used to build LargeIconServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  IOSChromeLargeIconServiceFactory(const IOSChromeLargeIconServiceFactory&) =
      delete;
  IOSChromeLargeIconServiceFactory& operator=(
      const IOSChromeLargeIconServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeLargeIconServiceFactory>;

  IOSChromeLargeIconServiceFactory();
  ~IOSChromeLargeIconServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
