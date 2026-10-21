// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

// Singleton that owns all ChromeAccountManagerServices and associates them with
// ProfileIOS.
class ChromeAccountManagerServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ChromeAccountManagerServiceFactory,
          ChromeAccountManagerService> {
 public:
  explicit ChromeAccountManagerServiceFactory(PassKey key);

 private:
  // ChromeAccountManagerServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_CHROME_ACCOUNT_MANAGER_SERVICE_FACTORY_H_
