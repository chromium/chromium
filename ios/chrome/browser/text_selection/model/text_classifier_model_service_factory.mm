// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_classifier_model_service_factory.h"

#import "base/no_destructor.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

// static
TextClassifierModelServiceFactory*
TextClassifierModelServiceFactory::GetInstance() {
  static base::NoDestructor<TextClassifierModelServiceFactory> instance;
  return instance.get();
}

// static
TextClassifierModelService* TextClassifierModelServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TextClassifierModelService>(
      profile, /*create=*/true);
}

TextClassifierModelServiceFactory::TextClassifierModelServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TextClassifierModel",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

TextClassifierModelServiceFactory::~TextClassifierModelServiceFactory() {}

std::unique_ptr<KeyedService>
TextClassifierModelServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    return nullptr;
  }

  // The optimization guide service must be available for the text classifier
  // model service to be created.
  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!opt_guide) {
    return nullptr;
  }
  return std::make_unique<TextClassifierModelService>(opt_guide);
}
