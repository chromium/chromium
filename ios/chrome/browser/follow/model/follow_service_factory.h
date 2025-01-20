// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class FollowService;
class ProfileIOS;

// Singleton that owns all FollowServices and associates them with
// ProfileIOS.
class FollowServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static FollowService* GetForProfile(ProfileIOS* profile);
  static FollowServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<FollowServiceFactory>;

  FollowServiceFactory();
  ~FollowServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_
