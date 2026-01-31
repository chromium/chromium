// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace supervised_user {

// Singleton that owns SupervisedUserService objects and associates
// them with Profiles.
class SupervisedUserUrlFilteringServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static SupervisedUserUrlFilteringService* GetForProfile(ProfileIOS* profile);

  static SupervisedUserUrlFilteringServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserUrlFilteringServiceFactory>;

  SupervisedUserUrlFilteringServiceFactory();
  ~SupervisedUserUrlFilteringServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_
