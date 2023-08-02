// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/language/web_view_language_model_manager_factory.h"

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
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

namespace {

void PrepareLanguageModels(WebViewBrowserState* const web_view_browser_state,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments. Note: there are currently no such experiments on iOS.
  manager->AddModel(language::LanguageModelManager::ModelType::FLUENT,
                    std::make_unique<language::FluentLanguageModel>(
                        web_view_browser_state->GetPrefs()));
  manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
}

}  // namespace

// static
WebViewLanguageModelManagerFactory*
WebViewLanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewLanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager*
WebViewLanguageModelManagerFactory::GetForBrowserState(
    WebViewBrowserState* const state) {
  return static_cast<language::LanguageModelManager*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

WebViewLanguageModelManagerFactory::WebViewLanguageModelManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "LanguageModelManager",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewLanguageModelManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* const context) const {
  WebViewBrowserState* const web_view_browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          web_view_browser_state->GetPrefs(),
          ApplicationContext::GetInstance()->GetApplicationLocale());
  PrepareLanguageModels(web_view_browser_state, manager.get());
  return manager;
}

web::BrowserState* WebViewLanguageModelManagerFactory::GetBrowserStateToUse(
    web::BrowserState* state) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(state);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
