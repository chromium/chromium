// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeAccountManagerService;
class ProfileIOS;

// Singleton that owns all ChromeAccountManagerServices and associates them with
// ProfileIOS.
class ChromeAccountManagerServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  ChromeAccountManagerServiceFactory(const BrowserStateKeyedServiceFactory&) =
      delete;
  ChromeAccountManagerServiceFactory& operator=(
      const BrowserStateKeyedServiceFactory&) = delete;

  static ChromeAccountManagerService* GetForProfile(ProfileIOS* profile);
  static ChromeAccountManagerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChromeAccountManagerServiceFactory>;

  ChromeAccountManagerServiceFactory();
  ~ChromeAccountManagerServiceFactory() override;

  // ChromeAccountManagerServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
