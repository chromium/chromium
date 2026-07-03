// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace personal_context {

class PersonalContextEnablementService;

class IOSPersonalContextEnablementServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static PersonalContextEnablementService* GetForProfile(ProfileIOS* profile);
  static IOSPersonalContextEnablementServiceFactory* GetInstance();

  IOSPersonalContextEnablementServiceFactory(
      const IOSPersonalContextEnablementServiceFactory&) = delete;
  IOSPersonalContextEnablementServiceFactory& operator=(
      const IOSPersonalContextEnablementServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSPersonalContextEnablementServiceFactory>;

  IOSPersonalContextEnablementServiceFactory();
  ~IOSPersonalContextEnablementServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace personal_context

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_FACTORY_H_
