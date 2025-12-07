// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class SigninErrorController;

namespace ios {
// Singleton that owns all SigninErrorControllers and associates them with
// ProfileIOS.
class SigninErrorControllerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SigninErrorController* GetForProfile(ProfileIOS* profile);
  static SigninErrorControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninErrorControllerFactory>;

  SigninErrorControllerFactory();
  ~SigninErrorControllerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
