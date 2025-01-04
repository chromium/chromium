// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class SigninClient;

// Singleton that owns all SigninClients and associates them with
// ProfileIOS.
class SigninClientFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SigninClient* GetForProfile(ProfileIOS* profile);
  static SigninClientFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninClientFactory>;

  SigninClientFactory();
  ~SigninClientFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_CLIENT_FACTORY_H_
