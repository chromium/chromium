// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_

#import "components/omnibox/browser/autocomplete_scoring_model_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

// A factory to create a unique `AutocompleteScoringModelService` per
// profile. Has a dependency on `OptimizationGuideServiceFactory`.
class AutocompleteScoringModelServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AutocompleteScoringModelServiceFactory,
          AutocompleteScoringModelService> {
 public:
  explicit AutocompleteScoringModelServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
