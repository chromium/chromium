// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_

#import "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class IOSAutofillAiPersonalContextAccessManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAutofillAiPersonalContextAccessManagerFactory,
          autofill::AutofillAiPersonalContextAccessManager> {
 public:
  explicit IOSAutofillAiPersonalContextAccessManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_FACTORY_H_
