// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class AutocompleteClassifier;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all AutocompleteClassifiers and associates them with
// ios::ChromeBrowserState.
class AutocompleteClassifierFactory : public BrowserStateKeyedServiceFactory {
 public:
  static AutocompleteClassifier* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static AutocompleteClassifierFactory* GetInstance();

  // Returns the default factory used to build AutocompleteClassifiers. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  ~AutocompleteClassifierFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteClassifierFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
