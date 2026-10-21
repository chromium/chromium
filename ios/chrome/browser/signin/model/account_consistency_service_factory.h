// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_

#import <memory>

#import "components/signin/ios/browser/account_consistency_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that creates the AccountConsistencyService(s) and associates those
// services  with profiles.
class AccountConsistencyServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AccountConsistencyServiceFactory,
          AccountConsistencyService> {
 public:
  explicit AccountConsistencyServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_CONSISTENCY_SERVICE_FACTORY_H_
