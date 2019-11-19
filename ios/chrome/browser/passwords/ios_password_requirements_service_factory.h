// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"

enum class ServiceAccessType;

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordRequirementsService;
}

// Singleton that owns all PasswordRequirementsService and associates them with
// ios::ChromeBrowserState.
class IOSPasswordRequirementsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordRequirementsService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state,
      ServiceAccessType access_type);

  static IOSPasswordRequirementsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSPasswordRequirementsServiceFactory>;

  IOSPasswordRequirementsServiceFactory();
  ~IOSPasswordRequirementsServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(IOSPasswordRequirementsServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
