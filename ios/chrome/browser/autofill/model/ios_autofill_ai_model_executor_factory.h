// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_

#import <memory>

#import "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns the AutofillAiModelExecutor responsible for managing
// calls to the AutofillAI server model via optimization guide infrastructure.
class IOSAutofillAiModelExecutorFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAutofillAiModelExecutorFactory,
          autofill::AutofillAiModelExecutor> {
 public:
  explicit IOSAutofillAiModelExecutorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_AI_MODEL_EXECUTOR_FACTORY_H_
