// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
WebViewUrlLanguageHistogramFactory*
WebViewUrlLanguageHistogramFactory::GetInstance() {
  static base::NoDestructor<WebViewUrlLanguageHistogramFactory> instance;
  return instance.get();
}

// static
language::UrlLanguageHistogram*
WebViewUrlLanguageHistogramFactory::GetForBrowserState(
    WebViewBrowserState* const state) {
  return static_cast<language::UrlLanguageHistogram*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

WebViewUrlLanguageHistogramFactory::WebViewUrlLanguageHistogramFactory()
    : BrowserStateKeyedServiceFactory(
          "UrlLanguageHistogram",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
WebViewUrlLanguageHistogramFactory::BuildServiceInstanceFor(
    web::BrowserState* const context) const {
  WebViewBrowserState* const web_view_browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<language::UrlLanguageHistogram>(
      web_view_browser_state->GetPrefs());
}

void WebViewUrlLanguageHistogramFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}

}  // namespace ios_web_view
