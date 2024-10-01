// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordRequirementsService;
}

// Singleton that owns all PasswordRequirementsService and associates them with
// ProfileIOS.
class IOSPasswordRequirementsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordRequirementsService* GetForBrowserState(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  static password_manager::PasswordRequirementsService* GetForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  static IOSPasswordRequirementsServiceFactory* GetInstance();

  IOSPasswordRequirementsServiceFactory(
      const IOSPasswordRequirementsServiceFactory&) = delete;
  IOSPasswordRequirementsServiceFactory& operator=(
      const IOSPasswordRequirementsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSPasswordRequirementsServiceFactory>;

  IOSPasswordRequirementsServiceFactory();
  ~IOSPasswordRequirementsServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
