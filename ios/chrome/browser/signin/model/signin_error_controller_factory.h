// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_

#import "components/signin/core/browser/signin_error_controller.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all SigninErrorControllers and associates them with
// ProfileIOS.
class SigninErrorControllerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SigninErrorControllerFactory,
                                                SigninErrorController> {
 public:
  explicit SigninErrorControllerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
