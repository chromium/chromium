// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GoogleGroupsManager;
class ProfileIOS;

class GoogleGroupsManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static GoogleGroupsManager* GetForProfile(ProfileIOS* profile);
  static GoogleGroupsManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<GoogleGroupsManagerFactory>;

  GoogleGroupsManagerFactory();
  ~GoogleGroupsManagerFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;

  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
