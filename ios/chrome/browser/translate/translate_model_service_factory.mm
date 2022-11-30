// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_model_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_model_service.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
TranslateModelServiceFactory* TranslateModelServiceFactory::GetInstance() {
  static base::NoDestructor<TranslateModelServiceFactory> instance;
  return instance.get();
}

// static
translate::TranslateModelService*
TranslateModelServiceFactory::GetForBrowserState(ChromeBrowserState* state) {
  return static_cast<translate::TranslateModelService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

TranslateModelServiceFactory::TranslateModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateModelService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

TranslateModelServiceFactory::~TranslateModelServiceFactory() {}

std::unique_ptr<KeyedService>
TranslateModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled() ||
      !optimization_guide::features::IsOptimizationTargetPredictionEnabled())
    return nullptr;
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  // The optimization guide service must be available for the translate model
  // service to be created.
  auto* opt_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(browser_state);
  if (opt_guide) {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    return std::make_unique<translate::TranslateModelService>(
        opt_guide, background_task_runner);
  }
  return nullptr;
}

web::BrowserState* TranslateModelServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
