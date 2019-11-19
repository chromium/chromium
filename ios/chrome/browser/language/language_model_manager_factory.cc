// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/language_model_manager_factory.h"

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "components/language/core/browser/fluent_language_model.h"
#include "components/language/core/browser/heuristic_language_model.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace {

void PrepareLanguageModels(ios::ChromeBrowserState* const chrome_state,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments.
  switch (language::GetOverrideLanguageModel()) {
    case language::OverrideLanguageModel::FLUENT:
      manager->AddModel(
          language::LanguageModelManager::ModelType::FLUENT,
          std::make_unique<language::FluentLanguageModel>(
              chrome_state->GetPrefs(), language::prefs::kAcceptLanguages));
      manager->SetPrimaryModel(
          language::LanguageModelManager::ModelType::FLUENT);
      break;
    case language::OverrideLanguageModel::HEURISTIC:
      manager->AddModel(language::LanguageModelManager::ModelType::HEURISTIC,
                        std::make_unique<language::HeuristicLanguageModel>(
                            chrome_state->GetPrefs(),
                            GetApplicationContext()->GetApplicationLocale(),
                            language::prefs::kAcceptLanguages,
                            language::prefs::kUserLanguageProfile));
      manager->SetPrimaryModel(
          language::LanguageModelManager::ModelType::HEURISTIC);
      break;
    case language::OverrideLanguageModel::DEFAULT:
    default:
      manager->AddModel(language::LanguageModelManager::ModelType::BASELINE,
                        std::make_unique<language::BaselineLanguageModel>(
                            chrome_state->GetPrefs(),
                            GetApplicationContext()->GetApplicationLocale(),
                            language::prefs::kAcceptLanguages));
      manager->SetPrimaryModel(
          language::LanguageModelManager::ModelType::BASELINE);
      break;
  }
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<LanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager* LanguageModelManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* const state) {
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
  ios::ChromeBrowserState* const chrome_state =
      ios::ChromeBrowserState::FromBrowserState(state);
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

void LanguageModelManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  if (base::FeatureList::IsEnabled(language::kUseHeuristicLanguageModel)) {
    registry->RegisterDictionaryPref(
        language::prefs::kUserLanguageProfile,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  }
}
