// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class AutofillAiPersonalContextAccessManager;
}

class IOSAutofillAiPersonalContextAccessManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static autofill::AutofillAiPersonalContextAccessManager* GetForProfile(
      ProfileIOS* profile);
  static IOSAutofillAiPersonalContextAccessManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      IOSAutofillAiPersonalContextAccessManagerFactory>;

  IOSAutofillAiPersonalContextAccessManagerFactory();
  ~IOSAutofillAiPersonalContextAccessManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
