// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace supervised_user {
// Factory helper method that returns true if we need to show the first
// time banner on the interstitial. The banner informs existing Desktop/iOS
// users about the application of parental controls.
bool ShouldShowFirstTimeBanner(ProfileIOS* profile);
}  // namespace supervised_user

// Singleton that owns SupervisedUserService objects and associates
// them with Profiles.
class SupervisedUserServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static supervised_user::SupervisedUserService* GetForProfile(
      ProfileIOS* profile);

  static SupervisedUserServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserServiceFactory>;

  SupervisedUserServiceFactory();
  ~SupervisedUserServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_
