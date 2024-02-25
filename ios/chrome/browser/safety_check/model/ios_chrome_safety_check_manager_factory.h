// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class IOSChromeSafetyCheckManager;
class KeyedService;

// Singleton that owns all IOSChromeSafetyCheckManager(s) and associates them
// with ChromeBrowserState.
class IOSChromeSafetyCheckManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeSafetyCheckManager* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static IOSChromeSafetyCheckManagerFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

  IOSChromeSafetyCheckManagerFactory(
      const IOSChromeSafetyCheckManagerFactory&) = delete;
  IOSChromeSafetyCheckManagerFactory& operator=(
      const IOSChromeSafetyCheckManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeSafetyCheckManagerFactory>;

  IOSChromeSafetyCheckManagerFactory();
  ~IOSChromeSafetyCheckManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_
