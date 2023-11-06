// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_bulk_leak_check_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {

// static
WebViewBulkLeakCheckServiceFactory*
WebViewBulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewBulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckServiceInterface*
WebViewBulkLeakCheckServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<password_manager::BulkLeakCheckServiceInterface*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewBulkLeakCheckServiceFactory::WebViewBulkLeakCheckServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BulkLeakCheckServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
}

WebViewBulkLeakCheckServiceFactory::~WebViewBulkLeakCheckServiceFactory() =
    default;

std::unique_ptr<KeyedService>
WebViewBulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<password_manager::BulkLeakCheckService>(
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      browser_state->GetSharedURLLoaderFactory());
}

}  // namespace ios_web_view
