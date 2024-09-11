// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class IOSProfileSessionDurationsService;

class IOSProfileSessionDurationsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static IOSProfileSessionDurationsService* GetForBrowserState(
      ProfileIOS* profile);

  static IOSProfileSessionDurationsService* GetForProfile(ProfileIOS* profile);
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
