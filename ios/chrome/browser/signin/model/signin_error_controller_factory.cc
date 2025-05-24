// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_error_controller_factory.h"

#include "base/memory/ptr_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

// static
SigninErrorController* SigninErrorControllerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SigninErrorController>(
      profile, /*create=*/true);
}

// static
SigninErrorControllerFactory* SigninErrorControllerFactory::GetInstance() {
  static base::NoDestructor<SigninErrorControllerFactory> instance;
  return instance.get();
}

SigninErrorControllerFactory::SigninErrorControllerFactory()
    : ProfileKeyedServiceFactoryIOS("SigninErrorController") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninErrorControllerFactory::~SigninErrorControllerFactory() = default;

std::unique_ptr<KeyedService>
SigninErrorControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<SigninErrorController>(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace ios
