// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AutocompleteClassifier;

namespace ios {
// Singleton that owns all AutocompleteClassifiers and associates them with
// profiles.
class AutocompleteClassifierFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static AutocompleteClassifier* GetForBrowserState(ProfileIOS* profile);

  static AutocompleteClassifier* GetForProfile(ProfileIOS* profile);
  static AutocompleteClassifierFactory* GetInstance();

  // Returns the default factory used to build AutocompleteClassifiers. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  AutocompleteClassifierFactory(const AutocompleteClassifierFactory&) = delete;
  AutocompleteClassifierFactory& operator=(
      const AutocompleteClassifierFactory&) = delete;

 private:
  friend class base::NoDestructor<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  ~AutocompleteClassifierFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
