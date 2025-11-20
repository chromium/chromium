// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_service.h"

#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

AutocompleteService::AutocompleteService(
    base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
        client_factory)
    : client_factory_(std::move(client_factory)) {}

AutocompleteService::~AutocompleteService() {}

void AutocompleteService::Shutdown() {
  controllers_.clear();
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
