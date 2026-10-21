// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_

#import <memory>

#import "components/language/core/browser/language_model_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Manages the language model for each profile. The particular language model
// provided depends on feature flags.
class LanguageModelManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          LanguageModelManagerFactory,
          language::LanguageModelManager> {
 public:
  explicit LanguageModelManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_LANGUAGE_MODEL_MANAGER_FACTORY_H_
