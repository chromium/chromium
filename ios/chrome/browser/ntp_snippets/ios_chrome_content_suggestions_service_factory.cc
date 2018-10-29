// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"

using ntp_snippets::ContentSuggestionsService;

// static
ContentSuggestionsService*
IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeContentSuggestionsServiceFactory*
IOSChromeContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeContentSuggestionsServiceFactory>::get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(
      &ntp_snippets::CreateChromeContentSuggestionsServiceWithProviders);
}

IOSChromeContentSuggestionsServiceFactory::
    IOSChromeContentSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

IOSChromeContentSuggestionsServiceFactory::
    ~IOSChromeContentSuggestionsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  if (tests_hook::DisableContentSuggestions()) {
    return ntp_snippets::CreateChromeContentSuggestionsService(browser_state);
  } else {
    return ntp_snippets::CreateChromeContentSuggestionsServiceWithProviders(
        browser_state);
  }
}

void IOSChromeContentSuggestionsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kArticlesForYouEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
