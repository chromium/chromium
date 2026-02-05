// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class EntityDataManager;
}

// Singleton that owns all EntityDataManagers and associates them with
// ProfileIOS.
class IOSAutofillEntityDataManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static autofill::EntityDataManager* GetForProfile(ProfileIOS* profile);
  static IOSAutofillEntityDataManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSAutofillEntityDataManagerFactory>;

  IOSAutofillEntityDataManagerFactory();
  ~IOSAutofillEntityDataManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_ENTITY_DATA_MANAGER_FACTORY_H_
