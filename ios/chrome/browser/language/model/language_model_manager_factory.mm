// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language/model/language_model_manager_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/language/core/browser/language_model.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/core/common/language_experiments.h"
#import "components/language/core/language_model/fluent_language_model.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

void PrepareLanguageModels(ProfileIOS* const profile,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments. Note: there are currently no such experiments on iOS.
  manager->AddModel(
      language::LanguageModelManager::ModelType::FLUENT,
      std::make_unique<language::FluentLanguageModel>(profile->GetPrefs()));
  manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
}

}  // namespace

// Use the original profile's language model even in Incognito mode.
LanguageModelManagerFactory::LanguageModelManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "LanguageModelManager",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
LanguageModelManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          profile->GetPrefs(), GetApplicationContext()
                                   ->GetApplicationLocaleStorage()
                                   ->GetTag()
                                   .tag_string());
  PrepareLanguageModels(profile, manager.get());
  return manager;
}
