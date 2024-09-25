// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AccountInvestigator;
class PrefRegistrySyncable;

namespace ios {

// Singleton that creates the AccountInvestigatorFactory(s) and associates those
// services with profiles.
class AccountInvestigatorFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns null if this profile cannot have an AccountInvestigatorFactory (for
  // example, if it is incognito).
  static AccountInvestigator* GetForProfile(ProfileIOS* profile);
  static AccountInvestigatorFactory* GetInstance();

  AccountInvestigatorFactory(const AccountInvestigatorFactory&) = delete;
  AccountInvestigatorFactory& operator=(const AccountInvestigatorFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AccountInvestigatorFactory>;

  AccountInvestigatorFactory();
  ~AccountInvestigatorFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_INVESTIGATOR_FACTORY_H_
