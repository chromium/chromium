// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_PERSONAL_DATA_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_PERSONAL_DATA_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace autofill {

class PersonalDataManager;

// Singleton that owns all PersonalDataManagers and associates them with
// profiles.
class PersonalDataManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static PersonalDataManager* GetForBrowserState(ProfileIOS* profile);

  static PersonalDataManager* GetForProfile(ProfileIOS* profile);
  static PersonalDataManagerFactory* GetInstance();

  PersonalDataManagerFactory(const PersonalDataManagerFactory&) = delete;
  PersonalDataManagerFactory& operator=(const PersonalDataManagerFactory&) =
      delete;

 private:
  friend class base::NoDestructor<PersonalDataManagerFactory>;

  PersonalDataManagerFactory();
  ~PersonalDataManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_PERSONAL_DATA_MANAGER_FACTORY_H_
