// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class FollowService;

// Singleton that owns all FollowServices and associates them with
// ProfileIOS.
class FollowServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static FollowService* GetForProfile(ProfileIOS* profile);
  static FollowServiceFactory* GetInstance();

  FollowServiceFactory(const FollowServiceFactory&) = delete;
  FollowServiceFactory& operator=(const FollowServiceFactory&) = delete;

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
