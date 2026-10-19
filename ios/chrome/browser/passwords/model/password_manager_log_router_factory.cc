// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/password_manager_log_router_factory.h"

#include <memory>
#include <utility>

#include "components/autofill/core/browser/logging/log_router.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

using autofill::LogRouter;

PasswordManagerLogRouterFactory::PasswordManagerLogRouterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "PasswordManagerInternalsService") {}

std::unique_ptr<KeyedService>
PasswordManagerLogRouterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<LogRouter>();
}

}  // namespace ios
