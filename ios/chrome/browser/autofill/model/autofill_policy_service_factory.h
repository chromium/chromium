// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_

#import "components/autofill/core/browser/permissions/autofill_policy_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace autofill {

class AutofillPolicyServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<AutofillPolicyServiceFactory,
                                                AutofillPolicyService> {
 public:
  explicit AutofillPolicyServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_
