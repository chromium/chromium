// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scoring_model_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_service.h"
#import "ios/chrome/browser/autocomplete/model/in_memory_url_index_factory.h"
#import "ios/chrome/browser/autocomplete/model/on_device_tail_model_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

// Factory for AutocompleteProviderClient.
std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient(
    base::WeakPtr<ProfileIOS> profile) {
  CHECK(profile);
  return std::make_unique<AutocompleteProviderClientImpl>(profile.get());
}

}  // namespace

// static
AutocompleteService* AutocompleteServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AutocompleteService>(
      profile, /*create=*/true);
}

// static
AutocompleteServiceFactory* AutocompleteServiceFactory::GetInstance() {
  static base::NoDestructor<AutocompleteServiceFactory> instance;
  return instance.get();
}

AutocompleteServiceFactory::AutocompleteServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AutocompleteService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kCreateService) {
  DependsOn(ios::AutocompleteClassifierFactory::GetInstance());
  DependsOn(ios::AutocompleteScoringModelServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::InMemoryURLIndexFactory::GetInstance());
  DependsOn(OnDeviceTailModelServiceFactory::GetInstance());
  DependsOn(ios::ProviderStateServiceFactory::GetInstance());
  DependsOn(RemoteSuggestionsServiceFactory::GetInstance());
  DependsOn(ios::ShortcutsBackendFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::TopSitesFactory::GetInstance());
  DependsOn(ios::ZeroSuggestCacheServiceFactory::GetInstance());
}

AutocompleteServiceFactory::~AutocompleteServiceFactory() = default;

std::unique_ptr<KeyedService>
AutocompleteServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ios::ShortcutsBackendFactory::GetForProfile(profile);
  return std::make_unique<AutocompleteService>(
      base::BindRepeating(&CreateAutocompleteProviderClient,
                          profile->AsWeakPtr()),
      shortcuts_backend.get());
}
