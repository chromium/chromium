// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class SigninProfileInfoUpdater;

// Owns all SigninProfileInfoUpdater instances and associates them with
// profiles.
class SigninProfileInfoUpdaterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SigninProfileInfoUpdater* GetForProfile(ProfileIOS* profile);
  static SigninProfileInfoUpdaterFactory* GetInstance();

 private:
  friend class base::NoDestructor<SigninProfileInfoUpdaterFactory>;

  SigninProfileInfoUpdaterFactory();
  ~SigninProfileInfoUpdaterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_FACTORY_H_
