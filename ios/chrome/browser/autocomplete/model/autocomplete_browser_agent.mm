// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"

#import <algorithm>

#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_prefetcher.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

AutocompleteBrowserAgent::AutocompleteBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

AutocompleteBrowserAgent::~AutocompleteBrowserAgent() {
  RemoveServices();
}

AutocompleteController* AutocompleteBrowserAgent::GetAutocompleteController(
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

OmniboxShortcutsHelper* AutocompleteBrowserAgent::GetOmniboxShortcutsHelper(
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

void AutocompleteBrowserAgent::RemoveServices() {
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

void AutocompleteBrowserAgent::RegisterWebStateListForPrefetching(
    OmniboxPresentationContext context,
    WebStateList* web_state_list,
    PageClassificationCallback classification_callback) {
  ZeroSuggestPrefetcher* prefetcher = [[ZeroSuggestPrefetcher alloc]
      initWithAutocompleteController:GetAutocompleteController(context)
                        webStateList:web_state_list
              classificationCallback:std::move(classification_callback)
                  disconnectCallback:
                      base::BindOnce(&AutocompleteBrowserAgent::
                                         UnregisterWebStateListForPrefetching,
                                     AsWeakPtr())];
  web_state_list_prefetchers_[web_state_list] = prefetcher;
}

void AutocompleteBrowserAgent::UnregisterWebStateListForPrefetching(
    WebStateList* web_state_list) {
  auto it = web_state_list_prefetchers_.find(web_state_list);
  if (it != web_state_list_prefetchers_.end()) {
    [it->second disconnect];
    web_state_list_prefetchers_.erase(it);
  }
}

void AutocompleteBrowserAgent::RegisterWebStateForPrefetching(
    OmniboxPresentationContext context,
    web::WebState* web_state,
    PageClassificationCallback classification_callback) {
  ZeroSuggestPrefetcher* prefetcher = [[ZeroSuggestPrefetcher alloc]
      initWithAutocompleteController:GetAutocompleteController(context)
                            webState:web_state
              classificationCallback:std::move(classification_callback)
                  disconnectCallback:base::BindOnce(
                                         &AutocompleteBrowserAgent::
                                             UnregisterWebStateForPrefetching,
                                         AsWeakPtr())];
  web_state_prefetchers_[web_state] = prefetcher;
}

void AutocompleteBrowserAgent::UnregisterWebStateForPrefetching(
    web::WebState* web_state) {
  auto it = web_state_prefetchers_.find(web_state);
  if (it != web_state_prefetchers_.end()) {
    [it->second disconnect];
    web_state_prefetchers_.erase(it);
  }
}

std::unique_ptr<AutocompleteController>
AutocompleteBrowserAgent::CreateAutocompleteController() {
  std::unique_ptr<AutocompleteProviderClient> provider_client =
      std::make_unique<AutocompleteProviderClientImpl>(browser_->GetProfile());

  int providers = AutocompleteClassifier::DefaultOmniboxProviders();

  return std::make_unique<AutocompleteController>(
      std::move(provider_client),
      AutocompleteControllerConfig{.provider_types = providers});
}

std::unique_ptr<OmniboxShortcutsHelper>
AutocompleteBrowserAgent::CreateOmniboxShortcutsHelper() {
  return std::make_unique<OmniboxShortcutsHelper>(
      ios::ShortcutsBackendFactory::GetForProfile(browser_->GetProfile())
          .get());
}
