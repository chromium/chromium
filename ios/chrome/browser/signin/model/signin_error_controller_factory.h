// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SigninErrorController;

namespace ios {
// Singleton that owns all SigninErrorControllers and associates them with
// ProfileIOS.
class SigninErrorControllerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SigninErrorController* GetForProfile(ProfileIOS* profile);
  static SigninErrorControllerFactory* GetInstance();

  SigninErrorControllerFactory(const SigninErrorControllerFactory&) = delete;
  SigninErrorControllerFactory& operator=(const SigninErrorControllerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<SigninErrorControllerFactory>;

  SigninErrorControllerFactory();
  ~SigninErrorControllerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_ERROR_CONTROLLER_FACTORY_H_
