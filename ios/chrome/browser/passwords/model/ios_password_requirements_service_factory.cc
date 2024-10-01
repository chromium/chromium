// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_password_requirements_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
password_manager::PasswordRequirementsService*
IOSPasswordRequirementsServiceFactory::GetForBrowserState(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  return GetForProfile(profile, access_type);
}

// static
password_manager::PasswordRequirementsService*
IOSPasswordRequirementsServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  return static_cast<password_manager::PasswordRequirementsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSPasswordRequirementsServiceFactory*
IOSPasswordRequirementsServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPasswordRequirementsServiceFactory> instance;
  return instance.get();
}

IOSPasswordRequirementsServiceFactory::IOSPasswordRequirementsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordRequirementsServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {}

IOSPasswordRequirementsServiceFactory::
    ~IOSPasswordRequirementsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSPasswordRequirementsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return password_manager::CreatePasswordRequirementsService(
      context->GetSharedURLLoaderFactory());
}

bool IOSPasswordRequirementsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
