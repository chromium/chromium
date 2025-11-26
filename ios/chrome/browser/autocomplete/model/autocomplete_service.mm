// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_service.h"

#import <algorithm>

#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_prefetcher.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

AutocompleteService::AutocompleteService(
    base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
        client_factory,
    ShortcutsBackend* shortcuts_backend)
    : client_factory_(std::move(client_factory)),
      shortcuts_backend_(shortcuts_backend) {}

AutocompleteService::~AutocompleteService() {}

void AutocompleteService::Shutdown() {
  RemoveServices();
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
  for (auto it = web_state_list_prefetchers_.begin();
       it != web_state_list_prefetchers_.end(); ++it) {
    [it->second disconnect];
  }
  for (auto it = web_state_prefetchers_.begin();
       it != web_state_prefetchers_.end(); ++it) {
    [it->second disconnect];
  }
  web_state_list_prefetchers_.clear();
  web_state_prefetchers_.clear();
  shortcuts_helpers_.clear();
  controllers_.clear();
}

void AutocompleteService::RegisterWebStateListForPrefetching(
    OmniboxPresentationContext context,
    WebStateList* web_state_list,
    PageClassificationCallback classification_callback) {
  ZeroSuggestPrefetcher* prefetcher = [[ZeroSuggestPrefetcher alloc]
      initWithAutocompleteController:GetAutocompleteController(context)
                        webStateList:web_state_list
              classificationCallback:std::move(classification_callback)
                  disconnectCallback:
                      base::BindOnce(&AutocompleteService::
                                         UnregisterWebStateListForPrefetching,
                                     AsWeakPtr())];
  web_state_list_prefetchers_[web_state_list] = prefetcher;
}

void AutocompleteService::UnregisterWebStateListForPrefetching(
    WebStateList* web_state_list) {
  auto it = web_state_list_prefetchers_.find(web_state_list);
  if (it != web_state_list_prefetchers_.end()) {
    [it->second disconnect];
    web_state_list_prefetchers_.erase(it);
  }
}

void AutocompleteService::RegisterWebStateForPrefetching(
    OmniboxPresentationContext context,
    web::WebState* web_state,
    PageClassificationCallback classification_callback) {
  ZeroSuggestPrefetcher* prefetcher = [[ZeroSuggestPrefetcher alloc]
      initWithAutocompleteController:GetAutocompleteController(context)
                            webState:web_state
              classificationCallback:std::move(classification_callback)
                  disconnectCallback:base::BindOnce(
                                         &AutocompleteService::
                                             UnregisterWebStateForPrefetching,
                                         AsWeakPtr())];
  web_state_prefetchers_[web_state] = prefetcher;
}

void AutocompleteService::UnregisterWebStateForPrefetching(
    web::WebState* web_state) {
  auto it = web_state_prefetchers_.find(web_state);
  if (it != web_state_prefetchers_.end()) {
    [it->second disconnect];
    web_state_prefetchers_.erase(it);
  }
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
