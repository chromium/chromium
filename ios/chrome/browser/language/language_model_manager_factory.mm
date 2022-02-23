// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/language_model_manager_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/language_model/fluent_language_model.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
