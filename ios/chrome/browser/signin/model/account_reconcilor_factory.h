// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_

#import <memory>

#import "components/signin/core/browser/account_reconcilor.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

// Singleton that owns all AccountReconcilors and associates them with browser
// states.
class AccountReconcilorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AccountReconcilorFactory,
                                                AccountReconcilor> {
 public:
  explicit AccountReconcilorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_RECONCILOR_FACTORY_H_
