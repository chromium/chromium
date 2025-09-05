// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language_detection/model/language_detection_model_loader_service_ios_factory.h"

#import "base/no_destructor.h"
#import "components/language_detection/core/browser/language_detection_model_service.h"
#import "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/language_detection/model/language_detection_model_service_factory.h"
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
LanguageDetectionModelLoaderServiceIOSFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          language_detection::LanguageDetectionModelLoaderServiceIOS>(
          profile, /*create=*/true);
}

LanguageDetectionModelLoaderServiceIOSFactory::
    LanguageDetectionModelLoaderServiceIOSFactory()
    : ProfileKeyedServiceFactoryIOS("LanguageDetectionModelLoaderServiceIOS",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(LanguageDetectionModelServiceFactory::GetInstance());
}

LanguageDetectionModelLoaderServiceIOSFactory::
    ~LanguageDetectionModelLoaderServiceIOSFactory() = default;

std::unique_ptr<KeyedService>
LanguageDetectionModelLoaderServiceIOSFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!translate::IsTFLiteLanguageDetectionEnabled()) {
    return nullptr;
  }
  return std::make_unique<
      language_detection::LanguageDetectionModelLoaderServiceIOS>(
      LanguageDetectionModelServiceFactory::GetForProfile(profile));
}
