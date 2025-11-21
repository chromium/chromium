// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_service.h"

#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

AutocompleteService::AutocompleteService(
    base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
        client_factory,
    ShortcutsBackend* shortcuts_backend)
    : client_factory_(std::move(client_factory)),
      shortcuts_backend_(shortcuts_backend) {}

AutocompleteService::~AutocompleteService() {}

void AutocompleteService::Shutdown() {
  controllers_.clear();
  shortcuts_helpers_.clear();
}

AutocompleteController* AutocompleteService::GetAutocompleteController(
    OmniboxPresentationContext context) {
  auto it = controllers_.find(context);
  if (it != controllers_.end()) {
    return it->second.get();
  }

  auto controller = CreateAutocompleteController();
  if (!controller) {
    return nullptr;
  }
  AutocompleteController* controller_ptr = controller.get();
  controllers_[context] = std::move(controller);
  return controller_ptr;
}

OmniboxShortcutsHelper* AutocompleteService::GetOmniboxShortcutsHelper(
    OmniboxPresentationContext context) {
  auto it = shortcuts_helpers_.find(context);
  if (it != shortcuts_helpers_.end()) {
    return it->second.get();
  }

  auto helper = CreateOmniboxShortcutsHelper();
  OmniboxShortcutsHelper* helper_ptr = helper.get();
  shortcuts_helpers_[context] = std::move(helper);
  return helper_ptr;
}

void AutocompleteService::RemoveServices() {
  controllers_.clear();
  shortcuts_helpers_.clear();
}

std::unique_ptr<AutocompleteController>
AutocompleteService::CreateAutocompleteController() {
  std::unique_ptr<AutocompleteProviderClient> provider_client =
      client_factory_.Run();
  if (!provider_client) {
    return nullptr;
  }

  int providers = AutocompleteClassifier::DefaultOmniboxProviders();

  return std::make_unique<AutocompleteController>(
      std::move(provider_client),
      AutocompleteControllerConfig{.provider_types = providers});
}

std::unique_ptr<OmniboxShortcutsHelper>
AutocompleteService::CreateOmniboxShortcutsHelper() {
  return std::make_unique<OmniboxShortcutsHelper>(shortcuts_backend_);
}
