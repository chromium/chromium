// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_scoring_model_service_factory.h"

#import <utility>

#import "components/omnibox/browser/autocomplete_scoring_model_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

AutocompleteScoringModelServiceFactory::AutocompleteScoringModelServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AutocompleteScoringModelService",
          ProfileSelection::kOwnInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AutocompleteScoringModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  return optimization_guide ? std::make_unique<AutocompleteScoringModelService>(
                                  optimization_guide)
                            : nullptr;
}

}  // namespace ios
