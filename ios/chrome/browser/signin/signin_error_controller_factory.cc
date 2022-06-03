// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_error_controller_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"

namespace ios {

// static
SigninErrorController* SigninErrorControllerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SigninErrorController*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninErrorControllerFactory* SigninErrorControllerFactory::GetInstance() {
  static base::NoDestructor<SigninErrorControllerFactory> instance;
  return instance.get();
}

SigninErrorControllerFactory::SigninErrorControllerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninErrorController",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninErrorControllerFactory::~SigninErrorControllerFactory() {
}

std::unique_ptr<KeyedService>
SigninErrorControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SigninErrorController>(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state));
}

}  // namespace ios
