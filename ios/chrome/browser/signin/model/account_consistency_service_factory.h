// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AccountConsistencyService;

namespace ios {
// Singleton that creates the AccountConsistencyService(s) and associates those
// services  with profiles.
class AccountConsistencyServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of AccountConsistencyService associated with this
  // profile (creating one if none exists). Returns null if this profile cannot
  // have an AccountConsistencyService (for example, if it is incognito).
  static AccountConsistencyService* GetForProfile(ProfileIOS* profile);

  // Returns an instance of the factory singleton.
  static AccountConsistencyServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountConsistencyServiceFactory>;

  AccountConsistencyServiceFactory();
  ~AccountConsistencyServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
