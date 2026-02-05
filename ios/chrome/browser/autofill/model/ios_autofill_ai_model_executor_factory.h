// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {
class AutofillAiModelExecutor;
}

// Singleton that owns the AutofillAiModelExecutor responsible for managing
// calls to the AutofillAI server model via optimization guide infrastructure.
class IOSAutofillAiModelExecutorFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSAutofillAiModelExecutorFactory* GetInstance();
  static autofill::AutofillAiModelExecutor* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSAutofillAiModelExecutorFactory>;

  IOSAutofillAiModelExecutorFactory();
  ~IOSAutofillAiModelExecutorFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
