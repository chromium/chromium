// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_policy_service_factory.h"

#import <utility>

#import "components/autofill/core/browser/permissions/autofill_policy_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace autofill {

AutofillPolicyServiceFactory::AutofillPolicyServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AutofillPolicyService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
AutofillPolicyServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<AutofillPolicyService>(profile->GetPrefs());
}

}  // namespace autofill
