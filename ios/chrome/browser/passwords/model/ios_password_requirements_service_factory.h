// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace password_manager {
class PasswordRequirementsService;
}

// Singleton that owns all PasswordRequirementsService and associates them with
// ProfileIOS.
class IOSPasswordRequirementsServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static password_manager::PasswordRequirementsService* GetForProfile(
      ProfileIOS* profile);
  static IOSPasswordRequirementsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPasswordRequirementsServiceFactory>;

  IOSPasswordRequirementsServiceFactory();
  ~IOSPasswordRequirementsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
