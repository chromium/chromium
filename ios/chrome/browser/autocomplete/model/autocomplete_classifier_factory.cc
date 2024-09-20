// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/autocomplete/model/in_memory_url_index_factory.h"
#include "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#include "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {
namespace {

std::unique_ptr<KeyedService> BuildAutocompleteClassifier(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<AutocompleteClassifier>(
      base::WrapUnique(new AutocompleteController(
          base::WrapUnique(new AutocompleteProviderClientImpl(profile)),
          AutocompleteClassifier::DefaultOmniboxProviders())),
      base::WrapUnique(new AutocompleteSchemeClassifierImpl));
}

}  // namespace

// static
AutocompleteClassifier* AutocompleteClassifierFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
AutocompleteClassifier* AutocompleteClassifierFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<AutocompleteClassifier*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
AutocompleteClassifierFactory* AutocompleteClassifierFactory::GetInstance() {
  static base::NoDestructor<AutocompleteClassifierFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
AutocompleteClassifierFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildAutocompleteClassifier);
}

AutocompleteClassifierFactory::AutocompleteClassifierFactory()
    : BrowserStateKeyedServiceFactory(
          "AutocompleteClassifier",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::InMemoryURLIndexFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::ShortcutsBackendFactory::GetInstance());
}

AutocompleteClassifierFactory::~AutocompleteClassifierFactory() {}

std::unique_ptr<KeyedService>
AutocompleteClassifierFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildAutocompleteClassifier(context);
}

web::BrowserState* AutocompleteClassifierFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool AutocompleteClassifierFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
