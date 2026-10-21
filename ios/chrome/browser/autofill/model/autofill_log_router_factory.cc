// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/model/autofill_log_router_factory.h"

#include <utility>

#include "components/autofill/core/browser/logging/log_router.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace autofill {

AutofillLogRouterFactory::AutofillLogRouterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "AutofillInternalsService") {}

std::unique_ptr<KeyedService> AutofillLogRouterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<LogRouter>();
}

}  // namespace autofill
