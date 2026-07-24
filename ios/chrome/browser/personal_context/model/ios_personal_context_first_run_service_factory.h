// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace personal_context {
class PersonalContextFirstRunService;
}

// KeyedServiceFactory for PersonalContextFirstRunService on iOS.
class IOSPersonalContextFirstRunServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static personal_context::PersonalContextFirstRunService* GetForProfile(
      ProfileIOS* profile);

  static IOSPersonalContextFirstRunServiceFactory* GetInstance();

  IOSPersonalContextFirstRunServiceFactory(
      const IOSPersonalContextFirstRunServiceFactory&) = delete;
  IOSPersonalContextFirstRunServiceFactory& operator=(
      const IOSPersonalContextFirstRunServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSPersonalContextFirstRunServiceFactory>;

  IOSPersonalContextFirstRunServiceFactory();
  ~IOSPersonalContextFirstRunServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_FACTORY_H_
