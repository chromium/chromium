// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/autocomplete/model/in_memory_url_index_factory.h"
#include "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#include "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {
namespace {

std::unique_ptr<KeyedService> BuildAutocompleteClassifier(ProfileIOS* profile) {
  return std::make_unique<AutocompleteClassifier>(
      std::make_unique<AutocompleteController>(
          std::make_unique<AutocompleteProviderClientImpl>(profile),
          AutocompleteControllerConfig{
              .provider_types =
                  AutocompleteClassifier::DefaultOmniboxProviders()}),
      std::make_unique<AutocompleteSchemeClassifierImpl>());
}

}  // namespace

// static
AutocompleteClassifierFactory::TestingFactory
AutocompleteClassifierFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildAutocompleteClassifier);
}

AutocompleteClassifierFactory::AutocompleteClassifierFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AutocompleteClassifier",
          ProfileSelection::kRedirectedInIncognito,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::InMemoryURLIndexFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::ShortcutsBackendFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AutocompleteClassifierFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildAutocompleteClassifier(profile);
}

}  // namespace ios
