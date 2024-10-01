// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language_detection/model/language_detection_model_loader_service_ios_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language_detection/core/browser/language_detection_model_service.h"
#import "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
LanguageDetectionModelLoaderServiceIOSFactory*
LanguageDetectionModelLoaderServiceIOSFactory::GetInstance() {
  static base::NoDestructor<LanguageDetectionModelLoaderServiceIOSFactory>
      instance;
  return instance.get();
}

// static
language_detection::LanguageDetectionModelLoaderServiceIOS*
LanguageDetectionModelLoaderServiceIOSFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
language_detection::LanguageDetectionModelLoaderServiceIOS*
LanguageDetectionModelLoaderServiceIOSFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<
      language_detection::LanguageDetectionModelLoaderServiceIOS*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

LanguageDetectionModelLoaderServiceIOSFactory::
    LanguageDetectionModelLoaderServiceIOSFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageDetectionModelLoaderServiceIOS",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(LanguageDetectionModelServiceFactory::GetInstance());
}

LanguageDetectionModelLoaderServiceIOSFactory::
    ~LanguageDetectionModelLoaderServiceIOSFactory() {}

std::unique_ptr<KeyedService>
LanguageDetectionModelLoaderServiceIOSFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<
      language_detection::LanguageDetectionModelLoaderServiceIOS>(
      LanguageDetectionModelServiceFactory::GetForProfile(profile));
}

web::BrowserState*
LanguageDetectionModelLoaderServiceIOSFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
