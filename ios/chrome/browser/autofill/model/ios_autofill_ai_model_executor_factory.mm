// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_ai_model_executor_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_ai_model_cache_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSAutofillAiModelExecutorFactory::IOSAutofillAiModelExecutorFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "AutofillAiModelExecutor") {
  DependsOn(IOSAutofillAiModelCacheFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSAutofillAiModelExecutorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiServerModel)) {
    return nullptr;
  }

  autofill::AutofillAiModelCache* model_cache =
      IOSAutofillAiModelCacheFactory::GetForProfile(profile);
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!model_cache || !optimization_guide) {
    return nullptr;
  }
  return std::make_unique<autofill::AutofillAiModelExecutorImpl>(
      model_cache, optimization_guide,
      optimization_guide->GetModelQualityLogsUploaderService());
}
