// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/url_language_histogram_factory.h"

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
UrlLanguageHistogramFactory* UrlLanguageHistogramFactory::GetInstance() {
  static base::NoDestructor<UrlLanguageHistogramFactory> instance;
  return instance.get();
}

// static
language::UrlLanguageHistogram* UrlLanguageHistogramFactory::GetForBrowserState(
    ChromeBrowserState* const state) {
  return static_cast<language::UrlLanguageHistogram*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

UrlLanguageHistogramFactory::UrlLanguageHistogramFactory()
    : BrowserStateKeyedServiceFactory(
          "UrlLanguageHistogram",
          BrowserStateDependencyManager::GetInstance()) {}

UrlLanguageHistogramFactory::~UrlLanguageHistogramFactory() {}

std::unique_ptr<KeyedService>
UrlLanguageHistogramFactory::BuildServiceInstanceFor(
    web::BrowserState* const context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<language::UrlLanguageHistogram>(
      browser_state->GetPrefs());
}

void UrlLanguageHistogramFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}
