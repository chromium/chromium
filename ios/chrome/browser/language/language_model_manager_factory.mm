// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language/language_model_manager_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/language/core/browser/language_model.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/core/common/language_experiments.h"
#import "components/language/core/language_model/fluent_language_model.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace {

void PrepareLanguageModels(ChromeBrowserState* const chrome_state,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments. Note: there are currently no such experiments on iOS.
  manager->AddModel(language::LanguageModelManager::ModelType::FLUENT,
                    std::make_unique<language::FluentLanguageModel>(
                        chrome_state->GetPrefs()));
  manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<LanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager* LanguageModelManagerFactory::GetForBrowserState(
    ChromeBrowserState* const state) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

LanguageModelManagerFactory::LanguageModelManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModelManager",
          BrowserStateDependencyManager::GetInstance()) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

std::unique_ptr<KeyedService>
LanguageModelManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* const state) const {
  ChromeBrowserState* const chrome_state =
      ChromeBrowserState::FromBrowserState(state);
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          chrome_state->GetPrefs(),
          GetApplicationContext()->GetApplicationLocale());
  PrepareLanguageModels(chrome_state, manager.get());
  return manager;
}

web::BrowserState* LanguageModelManagerFactory::GetBrowserStateToUse(
    web::BrowserState* const state) const {
  // Use the original profile's language model even in Incognito mode.
  return GetBrowserStateRedirectedInIncognito(state);
}
