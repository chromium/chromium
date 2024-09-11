// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/model/autofill_log_router_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace autofill {

// static
LogRouter* AutofillLogRouterFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
LogRouter* AutofillLogRouterFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
AutofillLogRouterFactory* AutofillLogRouterFactory::GetInstance() {
  static base::NoDestructor<AutofillLogRouterFactory> instance;
  return instance.get();
}

AutofillLogRouterFactory::AutofillLogRouterFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillInternalsService",
          BrowserStateDependencyManager::GetInstance()) {}

AutofillLogRouterFactory::~AutofillLogRouterFactory() = default;

std::unique_ptr<KeyedService> AutofillLogRouterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<LogRouter>();
}

}  // namespace autofill
