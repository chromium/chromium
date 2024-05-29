// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "base/check_is_test.h"
#import "base/functional/bind.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/browser_state.h"

namespace ios {

namespace {
// Returns a `unique_ptr` to the `SearchEngineChoiceService` class.
std::unique_ptr<KeyedService> BuildSearchEngineChoiceService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<search_engines::SearchEngineChoiceService>(
      CHECK_DEREF(browser_state->GetPrefs()),
      CHECK_DEREF(GetApplicationContext()->GetLocalState()),
      GetApplicationContext()->GetVariationsService());
}

}  // namespace

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {}

SearchEngineChoiceServiceFactory::~SearchEngineChoiceServiceFactory() = default;

bool SearchEngineChoiceServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
SearchEngineChoiceServiceFactory::GetDefaultFactory() {
  CHECK_IS_TEST();
  return base::BindRepeating(&BuildSearchEngineChoiceService);
}

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
  return BuildSearchEngineChoiceService(context);
}

web::BrowserState* SearchEngineChoiceServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
