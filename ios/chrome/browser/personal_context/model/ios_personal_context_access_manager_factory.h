// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class PersonalContextAccessManager;
}

class IOSPersonalContextAccessManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static autofill::PersonalContextAccessManager* GetForProfile(
      ProfileIOS* profile);
  static IOSPersonalContextAccessManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPersonalContextAccessManagerFactory>;

  IOSPersonalContextAccessManagerFactory();
  ~IOSPersonalContextAccessManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
