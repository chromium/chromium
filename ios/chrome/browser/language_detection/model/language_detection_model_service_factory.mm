// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/language_detection/core/browser/language_detection_model_service.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
LanguageDetectionModelServiceFactory*
LanguageDetectionModelServiceFactory::GetInstance() {
  static base::NoDestructor<LanguageDetectionModelServiceFactory> instance;
  return instance.get();
}

// static
language_detection::LanguageDetectionModelService*
LanguageDetectionModelServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          language_detection::LanguageDetectionModelService>(profile,
                                                             /*create=*/true);
}

LanguageDetectionModelServiceFactory::LanguageDetectionModelServiceFactory()
    : ProfileKeyedServiceFactoryIOS("LanguageDetectionModelService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

LanguageDetectionModelServiceFactory::~LanguageDetectionModelServiceFactory() =
    default;

std::unique_ptr<KeyedService>
LanguageDetectionModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled() ||
      !optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    return nullptr;
  }
  auto* opt_guide = OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!opt_guide) {
    // The optimization guide service must be available for the translate model
    // service to be created.
    return nullptr;
  }

  return std::make_unique<language_detection::LanguageDetectionModelService>(
      opt_guide, base::ThreadPool::CreateSequencedTaskRunner(
                     {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}
