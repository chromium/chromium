// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_password_requirements_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
password_manager::PasswordRequirementsService*
IOSPasswordRequirementsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  return static_cast<password_manager::PasswordRequirementsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
