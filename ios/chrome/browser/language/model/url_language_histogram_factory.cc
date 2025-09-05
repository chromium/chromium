// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/url_language_histogram_factory.h"

#include "base/no_destructor.h"
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
  return GetInstance()->GetServiceForProfileAs<language::UrlLanguageHistogram>(
      profile, /*create=*/true);
}

UrlLanguageHistogramFactory::UrlLanguageHistogramFactory()
    : ProfileKeyedServiceFactoryIOS("UrlLanguageHistogram") {}

UrlLanguageHistogramFactory::~UrlLanguageHistogramFactory() {}

std::unique_ptr<KeyedService>
UrlLanguageHistogramFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<language::UrlLanguageHistogram>(profile->GetPrefs());
}

void UrlLanguageHistogramFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* const registry) {
  language::UrlLanguageHistogram::RegisterProfilePrefs(registry);
}
