// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_scoring_model_service_factory.h"

#import "components/omnibox/browser/autocomplete_scoring_model_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

// static
AutocompleteScoringModelServiceFactory*
AutocompleteScoringModelServiceFactory::GetInstance() {
  static base::NoDestructor<AutocompleteScoringModelServiceFactory> instance;
  return instance.get();
}

// static
AutocompleteScoringModelService*
AutocompleteScoringModelServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AutocompleteScoringModelService>(
      profile, /*create=*/true);
}

AutocompleteScoringModelServiceFactory::AutocompleteScoringModelServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AutocompleteScoringModelService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

AutocompleteScoringModelServiceFactory::
    ~AutocompleteScoringModelServiceFactory() = default;

std::unique_ptr<KeyedService>
AutocompleteScoringModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  return optimization_guide ? std::make_unique<AutocompleteScoringModelService>(
                                  optimization_guide)
                            : nullptr;
}

}  // namespace ios
