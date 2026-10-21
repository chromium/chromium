// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_

#import "components/signin/core/browser/account_investigator.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class PrefRegistrySyncable;

namespace ios {

// Singleton that creates the AccountInvestigatorFactory(s) and associates those
// services with profiles.
class AccountInvestigatorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AccountInvestigatorFactory,
                                                AccountInvestigator> {
 public:
  explicit AccountInvestigatorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_
