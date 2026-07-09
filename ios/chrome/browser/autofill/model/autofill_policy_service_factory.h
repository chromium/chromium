// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {

class AutofillPolicyService;

class AutofillPolicyServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static AutofillPolicyService* GetForProfile(ProfileIOS* profile);
  static AutofillPolicyServiceFactory* GetInstance();

  AutofillPolicyServiceFactory(const AutofillPolicyServiceFactory&) = delete;
  AutofillPolicyServiceFactory& operator=(const AutofillPolicyServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AutofillPolicyServiceFactory>;

  AutofillPolicyServiceFactory();
  ~AutofillPolicyServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_POLICY_SERVICE_FACTORY_H_
