// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ChromeAccountManagerService;
class ProfileIOS;

// Singleton that owns all ChromeAccountManagerServices and associates them with
// ProfileIOS.
class ChromeAccountManagerServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ChromeAccountManagerService* GetForProfile(ProfileIOS* profile);
  static ChromeAccountManagerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChromeAccountManagerServiceFactory>;

  ChromeAccountManagerServiceFactory();
  ~ChromeAccountManagerServiceFactory() override;

  // ChromeAccountManagerServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
