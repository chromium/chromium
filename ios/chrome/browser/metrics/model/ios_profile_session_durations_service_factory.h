// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class IOSProfileSessionDurationsService;

class IOSProfileSessionDurationsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for `browser_state`.
  static IOSProfileSessionDurationsService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static IOSProfileSessionDurationsServiceFactory* GetInstance();

  IOSProfileSessionDurationsServiceFactory(
      const IOSProfileSessionDurationsServiceFactory&) = delete;
  IOSProfileSessionDurationsServiceFactory& operator=(
      const IOSProfileSessionDurationsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSProfileSessionDurationsServiceFactory>;

  IOSProfileSessionDurationsServiceFactory();
  ~IOSProfileSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
