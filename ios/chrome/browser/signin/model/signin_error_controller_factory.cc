// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_error_controller_factory.h"

#include <utility>

#include "components/signin/core/browser/signin_error_controller.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace ios {

SigninErrorControllerFactory::SigninErrorControllerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SigninErrorController") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SigninErrorControllerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SigninErrorController>(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace ios
