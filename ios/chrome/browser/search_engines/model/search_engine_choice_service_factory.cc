// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/browser_state.h"

namespace ios {

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {}

SearchEngineChoiceServiceFactory::~SearchEngineChoiceServiceFactory() = default;

// static
SearchEngineChoiceServiceFactory*
SearchEngineChoiceServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceServiceFactory> factory;
  return factory.get();
}

// static
search_engines::SearchEngineChoiceService*
SearchEngineChoiceServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<search_engines::SearchEngineChoiceService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

std::unique_ptr<KeyedService>
SearchEngineChoiceServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<search_engines::SearchEngineChoiceService>(
      CHECK_DEREF(browser_state->GetPrefs()));
}

}  // namespace ios
