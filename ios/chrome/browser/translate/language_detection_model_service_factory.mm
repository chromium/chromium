// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/language_detection_model_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/translate/core/common/translate_util.h"
#import "components/translate/ios/browser/language_detection_model_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/translate/translate_model_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
LanguageDetectionModelServiceFactory*
LanguageDetectionModelServiceFactory::GetInstance() {
  static base::NoDestructor<LanguageDetectionModelServiceFactory> instance;
  return instance.get();
}

// static
translate::LanguageDetectionModelService*
LanguageDetectionModelServiceFactory::GetForBrowserState(
    ChromeBrowserState* state) {
  return static_cast<translate::LanguageDetectionModelService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageDetectionModelServiceFactory::LanguageDetectionModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageDetectionModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(TranslateModelServiceFactory::GetInstance());
}

LanguageDetectionModelServiceFactory::~LanguageDetectionModelServiceFactory() {}

std::unique_ptr<KeyedService>
LanguageDetectionModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled())
    return nullptr;
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  auto* translate_model_service =
      TranslateModelServiceFactory::GetForBrowserState(browser_state);
  return std::make_unique<translate::LanguageDetectionModelService>(
      translate_model_service, background_task_runner);
}

web::BrowserState* LanguageDetectionModelServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
