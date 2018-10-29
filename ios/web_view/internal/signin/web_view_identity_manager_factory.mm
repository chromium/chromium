// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/web_view/internal/signin/web_view_account_tracker_service_factory.h"
#include "ios/web_view/internal/signin/web_view_gaia_cookie_manager_service_factory.h"
#include "ios/web_view/internal/signin/web_view_oauth2_token_service_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to
// WebViewBrowserState without communicating with
// {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  explicit IdentityManagerWrapper(WebViewBrowserState* browser_state)
      : identity::IdentityManager(
            WebViewSigninManagerFactory::GetForBrowserState(browser_state),
            WebViewOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
            WebViewAccountTrackerServiceFactory::GetForBrowserState(
                browser_state),
            WebViewGaiaCookieManagerServiceFactory::GetForBrowserState(
                browser_state),
            std::make_unique<identity::PrimaryAccountMutatorImpl>(
                WebViewAccountTrackerServiceFactory::GetForBrowserState(
                    browser_state),
                WebViewSigninManagerFactory::GetForBrowserState(
                    browser_state))) {}
};

WebViewIdentityManagerFactory::WebViewIdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAccountTrackerServiceFactory::GetInstance());
  DependsOn(WebViewGaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(WebViewOAuth2TokenServiceFactory::GetInstance());
  DependsOn(WebViewSigninManagerFactory::GetInstance());
}

WebViewIdentityManagerFactory::~WebViewIdentityManagerFactory() {}

// static
identity::IdentityManager* WebViewIdentityManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewIdentityManagerFactory* WebViewIdentityManagerFactory::GetInstance() {
  return base::Singleton<WebViewIdentityManagerFactory>::get();
}

std::unique_ptr<KeyedService>
WebViewIdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<IdentityManagerWrapper>(
      WebViewBrowserState::FromBrowserState(browser_state));
}

}  // namespace ios_web_view
