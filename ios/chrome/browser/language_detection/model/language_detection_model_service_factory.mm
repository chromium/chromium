// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language_detection/core/browser/language_detection_model_service.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
LanguageDetectionModelServiceFactory*
LanguageDetectionModelServiceFactory::GetInstance() {
  static base::NoDestructor<LanguageDetectionModelServiceFactory> instance;
  return instance.get();
}

// static
language_detection::LanguageDetectionModelService*
LanguageDetectionModelServiceFactory::GetForProfile(ProfileIOS* state) {
  return static_cast<language_detection::LanguageDetectionModelService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageDetectionModelServiceFactory::LanguageDetectionModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageDetectionModelService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

LanguageDetectionModelServiceFactory::~LanguageDetectionModelServiceFactory() {}

std::unique_ptr<KeyedService>
LanguageDetectionModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled() ||
      !optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide = OptimizationGuideServiceFactory::GetForProfile(profile);
  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<language_detection::LanguageDetectionModelService>(
        opt_guide, background_task_runner);
  }
  return nullptr;
}

web::BrowserState* LanguageDetectionModelServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
