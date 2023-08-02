// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_requirements_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {

// static
password_manager::PasswordRequirementsService*
WebViewPasswordRequirementsServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  return static_cast<password_manager::PasswordRequirementsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewPasswordRequirementsServiceFactory*
WebViewPasswordRequirementsServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewPasswordRequirementsServiceFactory> instance;
  return instance.get();
}

WebViewPasswordRequirementsServiceFactory::
    WebViewPasswordRequirementsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordRequirementsServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewPasswordRequirementsServiceFactory::
    ~WebViewPasswordRequirementsServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewPasswordRequirementsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return password_manager::CreatePasswordRequirementsService(
      context->GetSharedURLLoaderFactory());
}

bool WebViewPasswordRequirementsServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

}  // namespace ios_web_view
