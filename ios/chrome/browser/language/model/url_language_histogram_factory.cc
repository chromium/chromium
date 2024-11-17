// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/url_language_histogram_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
UrlLanguageHistogramFactory* UrlLanguageHistogramFactory::GetInstance() {
  static base::NoDestructor<UrlLanguageHistogramFactory> instance;
  return instance.get();
}

// static
language::UrlLanguageHistogram* UrlLanguageHistogramFactory::GetForProfile(
    ProfileIOS* const profile) {
  return static_cast<language::UrlLanguageHistogram*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

UrlLanguageHistogramFactory::UrlLanguageHistogramFactory()
    : BrowserStateKeyedServiceFactory(
          "UrlLanguageHistogram",
          BrowserStateDependencyManager::GetInstance()) {}

UrlLanguageHistogramFactory::~UrlLanguageHistogramFactory() {}

std::unique_ptr<KeyedService>
UrlLanguageHistogramFactory::BuildServiceInstanceFor(
    web::BrowserState* const context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<language::UrlLanguageHistogram>(profile->GetPrefs());
}

void UrlLanguageHistogramFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}
