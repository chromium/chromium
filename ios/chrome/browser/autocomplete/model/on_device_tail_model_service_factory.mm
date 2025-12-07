// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/on_device_tail_model_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/browser/on_device_tail_model_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
OnDeviceTailModelServiceFactory*
OnDeviceTailModelServiceFactory::GetInstance() {
  static base::NoDestructor<OnDeviceTailModelServiceFactory> instance;
  return instance.get();
}

// static
OnDeviceTailModelService* OnDeviceTailModelServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<OnDeviceTailModelService>(
      profile, /*create=*/true);
}

OnDeviceTailModelServiceFactory::OnDeviceTailModelServiceFactory()
    : ProfileKeyedServiceFactoryIOS("OnDeviceTailModelService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

OnDeviceTailModelServiceFactory::~OnDeviceTailModelServiceFactory() = default;

std::unique_ptr<KeyedService>
OnDeviceTailModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  const std::string locale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();
  if (!OmniboxFieldTrial::IsOnDeviceTailSuggestEnabled(locale)) {
    return nullptr;
  }
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  return optimization_guide
             ? std::make_unique<OnDeviceTailModelService>(optimization_guide)
             : nullptr;
}
