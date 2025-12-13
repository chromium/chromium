// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BwgService;
class ProfileIOS;

// Singleton that owns all BwgServices and associates them with a Profile.
class BwgServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BwgService* GetForProfile(ProfileIOS* profile);
  static BwgServiceFactory* GetInstance();

  // Returns the default factory used to build BwgService. Can be registered
  // with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<BwgServiceFactory>;

  BwgServiceFactory();
  ~BwgServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_FACTORY_H_
