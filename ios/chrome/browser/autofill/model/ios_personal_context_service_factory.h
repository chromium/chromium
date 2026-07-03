// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace personal_context {

class PersonalContextService;

class IOSPersonalContextServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PersonalContextService* GetForProfile(ProfileIOS* profile);
  static IOSPersonalContextServiceFactory* GetInstance();

  IOSPersonalContextServiceFactory(const IOSPersonalContextServiceFactory&) =
      delete;
  IOSPersonalContextServiceFactory& operator=(
      const IOSPersonalContextServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSPersonalContextServiceFactory>;

  IOSPersonalContextServiceFactory();
  ~IOSPersonalContextServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace personal_context

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_PERSONAL_CONTEXT_SERVICE_FACTORY_H_
