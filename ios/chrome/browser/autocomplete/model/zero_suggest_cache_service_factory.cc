// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ZeroSuggestCacheService* ZeroSuggestCacheServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ZeroSuggestCacheService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<ZeroSuggestCacheService>(
      std::make_unique<AutocompleteSchemeClassifierImpl>(), profile->GetPrefs(),
      OmniboxFieldTrial::kZeroSuggestCacheMaxSize.Get());
}

}  // namespace ios
