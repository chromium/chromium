// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_service_factory.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

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
                                    TestingCreation::kCreateService) {}

AutocompleteServiceFactory::~AutocompleteServiceFactory() = default;

std::unique_ptr<KeyedService>
AutocompleteServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<AutocompleteService>(base::BindRepeating(
      &CreateAutocompleteProviderClient, profile->AsWeakPtr()));
}
