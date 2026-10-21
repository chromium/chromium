// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#import "components/password_manager/core/browser/password_requirements_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all PasswordRequirementsService and associates them with
// ProfileIOS.
class IOSPasswordRequirementsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSPasswordRequirementsServiceFactory,
          password_manager::PasswordRequirementsService> {
 public:
  explicit IOSPasswordRequirementsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
