// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_

#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace supervised_user {

// Singleton that owns SupervisedUserService objects and associates
// them with Profiles.
class SupervisedUserServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SupervisedUserServiceFactory,
                                                SupervisedUserService> {
 public:
  explicit SupervisedUserServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_FACTORY_H_
