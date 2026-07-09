// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_policy_service_factory.h"

#import "base/no_destructor.h"
#import "components/autofill/core/browser/permissions/autofill_policy_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace autofill {

// static
AutofillPolicyService* AutofillPolicyServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AutofillPolicyService>(
      profile, /*create=*/true);
}

// static
AutofillPolicyServiceFactory* AutofillPolicyServiceFactory::GetInstance() {
  static base::NoDestructor<AutofillPolicyServiceFactory> instance;
  return instance.get();
}

AutofillPolicyServiceFactory::AutofillPolicyServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AutofillPolicyService",
                                    ProfileSelection::kRedirectedInIncognito) {}

AutofillPolicyServiceFactory::~AutofillPolicyServiceFactory() = default;

std::unique_ptr<KeyedService>
AutofillPolicyServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<AutofillPolicyService>(profile->GetPrefs());
}

}  // namespace autofill
