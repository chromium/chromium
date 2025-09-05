// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"

#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ZeroSuggestCacheService>(
      profile, /*create=*/true);
}

// static
ZeroSuggestCacheServiceFactory* ZeroSuggestCacheServiceFactory::GetInstance() {
  static base::NoDestructor<ZeroSuggestCacheServiceFactory> instance;
  return instance.get();
}

ZeroSuggestCacheServiceFactory::ZeroSuggestCacheServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ZeroSuggestCacheService") {}

ZeroSuggestCacheServiceFactory::~ZeroSuggestCacheServiceFactory() = default;

std::unique_ptr<KeyedService>
ZeroSuggestCacheServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ZeroSuggestCacheService>(
      std::make_unique<AutocompleteSchemeClassifierImpl>(), profile->GetPrefs(),
      OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

}  // namespace ios
