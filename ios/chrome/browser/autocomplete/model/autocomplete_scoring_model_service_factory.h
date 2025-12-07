// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AutocompleteScoringModelService;
class KeyedService;
class ProfileIOS;

namespace ios {

// A factory to create a unique `AutocompleteScoringModelService` per
// profile. Has a dependency on `OptimizationGuideKeyedServiceFactory`.
class AutocompleteScoringModelServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static AutocompleteScoringModelService* GetForProfile(ProfileIOS* profile);
  static AutocompleteScoringModelServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AutocompleteScoringModelServiceFactory>;

  AutocompleteScoringModelServiceFactory();
  ~AutocompleteScoringModelServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
