// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_manager_factory.h"

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator_impl.h"

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to
// ChromeBrowserState without communicating with
// {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  explicit IdentityManagerWrapper(ios::ChromeBrowserState* browser_state)
      : identity::IdentityManager(
            ios::SigninManagerFactory::GetForBrowserState(browser_state),
            ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state),
            ios::AccountTrackerServiceFactory::GetForBrowserState(
                browser_state),
            ios::GaiaCookieManagerServiceFactory::GetForBrowserState(
                browser_state),
            std::make_unique<identity::PrimaryAccountMutatorImpl>(
                ios::AccountTrackerServiceFactory::GetForBrowserState(
                    browser_state),
                ios::SigninManagerFactory::GetForBrowserState(browser_state))) {
  }
};

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
  DependsOn(ios::GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  return base::Singleton<IdentityManagerFactory>::get();
}

std::unique_ptr<KeyedService> IdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<IdentityManagerWrapper>(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
