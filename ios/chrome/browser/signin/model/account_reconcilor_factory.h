// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AccountReconcilor;

namespace ios {

// Singleton that owns all AccountReconcilors and associates them with browser
// states.
class AccountReconcilorFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of AccountReconcilor associated with this profile
  // (creating one if none exists). Returns null if this profile cannot have an
  // GaiaCookieManagerService (for example, if it is incognito).
  static AccountReconcilor* GetForProfile(ProfileIOS* profile);

  // Returns an instance of the factory singleton.
  static AccountReconcilorFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountReconcilorFactory>;

  AccountReconcilorFactory();
  ~AccountReconcilorFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
