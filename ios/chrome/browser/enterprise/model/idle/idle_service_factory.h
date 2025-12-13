// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_idle {

// Singleton that owns all IdleServices and associates them with
// ProfileIOS.
class IdleServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static IdleService* GetForProfile(ProfileIOS* profile);
  static IdleServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IdleServiceFactory>;

  IdleServiceFactory();
  ~IdleServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;

  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_
