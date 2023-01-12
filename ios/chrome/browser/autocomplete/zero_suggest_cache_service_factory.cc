// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace ios {

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ZeroSuggestCacheService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ZeroSuggestCacheServiceFactory* ZeroSuggestCacheServiceFactory::GetInstance() {
  static base::NoDestructor<ZeroSuggestCacheServiceFactory> instance;
  return instance.get();
}

ZeroSuggestCacheServiceFactory::ZeroSuggestCacheServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ZeroSuggestCacheService",
          BrowserStateDependencyManager::GetInstance()) {}

ZeroSuggestCacheServiceFactory::~ZeroSuggestCacheServiceFactory() = default;

std::unique_ptr<KeyedService>
ZeroSuggestCacheServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<ZeroSuggestCacheService>(
      browser_state->GetPrefs(),
      OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

}  // namespace ios
