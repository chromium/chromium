// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/account_reconcilor_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/web/common/features.h"

namespace ios {

AccountReconcilorFactory::AccountReconcilorFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountReconcilor",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

AccountReconcilorFactory::~AccountReconcilorFactory() {}

// static
AccountReconcilor* AccountReconcilorFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<AccountReconcilor*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AccountReconcilorFactory* AccountReconcilorFactory::GetInstance() {
  static base::NoDestructor<AccountReconcilorFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> AccountReconcilorFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  auto* identity_manager =
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state);
  std::unique_ptr<AccountReconcilor> reconcilor(new AccountReconcilor(
      identity_manager,
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      std::make_unique<signin::MirrorAccountReconcilorDelegate>(
          identity_manager)));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

}  // namespace ios
