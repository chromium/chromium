// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class LevelUpService;

// Singleton that owns all LevelUpServices and associates them with
// profiles.
class LevelUpServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static LevelUpService* GetForProfile(ProfileIOS* profile);
  static LevelUpServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<LevelUpServiceFactory>;

  LevelUpServiceFactory();
  ~LevelUpServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_FACTORY_H_
