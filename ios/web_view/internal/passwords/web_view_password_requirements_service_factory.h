// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordRequirementsService;
}  // namespace password_manager

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all PasswordRequirementsService and associates them with
// WebViewBrowserStates.
class WebViewPasswordRequirementsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordRequirementsService* GetForBrowserState(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebViewPasswordRequirementsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewPasswordRequirementsServiceFactory>;

  WebViewPasswordRequirementsServiceFactory();
  ~WebViewPasswordRequirementsServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  WebViewPasswordRequirementsServiceFactory(
      const WebViewPasswordRequirementsServiceFactory&) = delete;
  WebViewPasswordRequirementsServiceFactory& operator=(
      const WebViewPasswordRequirementsServiceFactory&) = delete;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
