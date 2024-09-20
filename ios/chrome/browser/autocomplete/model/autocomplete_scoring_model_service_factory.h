// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;
class AutocompleteScoringModelService;

namespace content {
class BrowserState;
}  // namespace content

namespace ios {

// A factory to create a unique `AutocompleteScoringModelService` per
// profile. Has a dependency on `OptimizationGuideKeyedServiceFactory`.
class AutocompleteScoringModelServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static AutocompleteScoringModelService* GetForBrowserState(
      ProfileIOS* profile);

  static AutocompleteScoringModelService* GetForProfile(ProfileIOS* profile);
  static AutocompleteScoringModelServiceFactory* GetInstance();

  // Disallow copy/assign.
  AutocompleteScoringModelServiceFactory(
      const AutocompleteScoringModelServiceFactory&) = delete;
  AutocompleteScoringModelServiceFactory& operator=(
      const AutocompleteScoringModelServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AutocompleteScoringModelServiceFactory>;

  AutocompleteScoringModelServiceFactory();
  ~AutocompleteScoringModelServiceFactory() override;

  // `BrowserContextKeyedServiceFactory` overrides.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCORING_MODEL_SERVICE_FACTORY_H_
