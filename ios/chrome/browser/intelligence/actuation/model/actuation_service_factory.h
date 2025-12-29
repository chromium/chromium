// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ActuationService;
class ProfileIOS;

// Singleton that owns all ActuationServices and associates them with
// ProfileIOS.
class ActuationServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ActuationService* GetForProfile(ProfileIOS* profile);
  static ActuationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ActuationServiceFactory>;

  ActuationServiceFactory();
  ~ActuationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_FACTORY_H_
