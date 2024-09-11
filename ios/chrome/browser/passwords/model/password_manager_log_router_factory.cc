// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/password_manager_log_router_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

using autofill::LogRouter;

// static
LogRouter* PasswordManagerLogRouterFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
LogRouter* PasswordManagerLogRouterFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
PasswordManagerLogRouterFactory*
PasswordManagerLogRouterFactory::GetInstance() {
  static base::NoDestructor<PasswordManagerLogRouterFactory> instance;
  return instance.get();
}

PasswordManagerLogRouterFactory::PasswordManagerLogRouterFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordManagerInternalsService",
          BrowserStateDependencyManager::GetInstance()) {}

PasswordManagerLogRouterFactory::~PasswordManagerLogRouterFactory() {}

std::unique_ptr<KeyedService>
PasswordManagerLogRouterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<LogRouter>();
}

}  // namespace ios
