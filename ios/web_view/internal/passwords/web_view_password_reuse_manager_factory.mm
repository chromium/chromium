// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_reuse_manager_factory.h"

#import "base/no_destructor.h"
#import "build/build_config.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/password_reuse_detector_impl.h"
#import "components/password_manager/core/browser/password_reuse_manager_impl.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"

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
  DependsOn(WebViewProfilePasswordStoreFactory::GetInstance());
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

  reuse_manager->Init(
      browser_state->GetPrefs(),
      ios_web_view::ApplicationContext::GetInstance()->GetLocalState(),
      WebViewProfilePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      std::make_unique<password_manager::PasswordReuseDetectorImpl>());
  return reuse_manager;
}

web::BrowserState* WebViewPasswordReuseManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
