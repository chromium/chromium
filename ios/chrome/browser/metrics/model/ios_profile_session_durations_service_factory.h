// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class IOSProfileSessionDurationsService;
class ProfileIOS;

// A factory that owns all IOSProfileSessionDurationsService and associate
// the to ProfileIOS instances.
class IOSProfileSessionDurationsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSProfileSessionDurationsService* GetForProfile(ProfileIOS* profile);
  static IOSProfileSessionDurationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSProfileSessionDurationsServiceFactory>;

  IOSProfileSessionDurationsServiceFactory();
  ~IOSProfileSessionDurationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
