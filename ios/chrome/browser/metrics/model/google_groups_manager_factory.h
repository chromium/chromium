// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_

#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class GoogleGroupsManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<GoogleGroupsManagerFactory,
                                                GoogleGroupsManager> {
 public:
  explicit GoogleGroupsManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;

  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
