// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AutocompleteClassifier;
class ProfileIOS;

namespace ios {

// Singleton that owns all AutocompleteClassifiers and associates them with
// profiles.
class AutocompleteClassifierFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static AutocompleteClassifier* GetForProfile(ProfileIOS* profile);
  static AutocompleteClassifierFactory* GetInstance();

  // Returns the default factory used to build AutocompleteClassifiers. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  ~AutocompleteClassifierFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
