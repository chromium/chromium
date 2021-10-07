// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/passwords/web_view_password_reuse_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
WebViewPasswordReuseManagerFactory*
WebViewPasswordReuseManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewPasswordReuseManagerFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordReuseManager*
WebViewPasswordReuseManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordReuseDetectionEnabled)) {
    return nullptr;
  }

  return static_cast<password_manager::PasswordReuseManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewPasswordReuseManagerFactory::WebViewPasswordReuseManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordReuseManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewPasswordStoreFactory::GetInstance());
}

WebViewPasswordReuseManagerFactory::~WebViewPasswordReuseManagerFactory() =
    default;

std::unique_ptr<KeyedService>
WebViewPasswordReuseManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled));

  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<password_manager::PasswordReuseManager> reuse_manager =
      std::make_unique<password_manager::PasswordReuseManagerImpl>();

  reuse_manager->Init(browser_state->GetPrefs(),
                      WebViewPasswordStoreFactory::GetForBrowserState(
                          browser_state, ServiceAccessType::EXPLICIT_ACCESS)
                          .get());
  return reuse_manager;
}

web::BrowserState* WebViewPasswordReuseManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
