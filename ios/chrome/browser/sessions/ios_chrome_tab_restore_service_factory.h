// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace sessions {
class TabRestoreService;
}

// Singleton that owns all TabRestoreServices and associates them with
// ChromeBrowserStates.
class IOSChromeTabRestoreServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static sessions::TabRestoreService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static IOSChromeTabRestoreServiceFactory* GetInstance();

  // Returns the default factory used to build TabRestoreServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  IOSChromeTabRestoreServiceFactory(const IOSChromeTabRestoreServiceFactory&) =
      delete;
  IOSChromeTabRestoreServiceFactory& operator=(
      const IOSChromeTabRestoreServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeTabRestoreServiceFactory>;

  IOSChromeTabRestoreServiceFactory();
  ~IOSChromeTabRestoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
