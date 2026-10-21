// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/on_device_tail_model_service_factory.h"

#import <memory>
#import <utility>

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/browser/on_device_tail_model_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

OnDeviceTailModelServiceFactory::OnDeviceTailModelServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "OnDeviceTailModelService",
          ProfileSelection::kOwnInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
OnDeviceTailModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  const std::string locale(GetApplicationContext()
                               ->GetApplicationLocaleStorage()
                               ->GetTag()
                               .tag_string());
  if (!OmniboxFieldTrial::IsOnDeviceTailSuggestEnabled(locale)) {
    return nullptr;
  }
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  return optimization_guide
             ? std::make_unique<OnDeviceTailModelService>(optimization_guide)
             : nullptr;
}
