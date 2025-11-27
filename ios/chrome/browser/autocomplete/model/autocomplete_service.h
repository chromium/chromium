// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_

#import <map>
#import <memory>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/autocomplete/model/omnibox_shortcuts_helper.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"

@class ZeroSuggestPrefetcher;
class AutocompleteController;
class AutocompleteProviderClient;
class ShortcutsBackend;

/// Keyed Service that owns and manages long-lived Omnibox objects
/// (AutocompleteController, etc.) per presentation context.
class AutocompleteService : public KeyedService {
 public:
  explicit AutocompleteService(
      base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
          client_factory,
      ShortcutsBackend* shortcuts_backend);
  ~AutocompleteService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the AutocompleteController for the given context.
  // If it doesn't exist, it creates one.
  AutocompleteController* GetAutocompleteController(
      OmniboxPresentationContext context);

  // Returns the OmniboxShortcutsHelper for the given context.
  // If it doesn't exist, it creates one.
  OmniboxShortcutsHelper* GetOmniboxShortcutsHelper(
      OmniboxPresentationContext context);

  // Removes AutocompleteController and Shortcuts helper for all context.
  void RemoveServices();

  using PageClassificationCallback =
      base::RepeatingCallback<metrics::OmniboxEventProto::PageClassification()>;

  // Registers a WebStateList for prefetching.
  // `context` is the presentation context for which to prefetch.
  // `web_state_list` is the WebStateList to observe.
  // `classification_callback` is called to compute page classification.
  void RegisterWebStateListForPrefetching(
      OmniboxPresentationContext context,
      WebStateList* web_state_list,
      PageClassificationCallback classification_callback);

  // Unregisters a WebStateList for prefetching.
  void UnregisterWebStateListForPrefetching(WebStateList* web_state_list);

  // Registers a single WebState for prefetching.
  void RegisterWebStateForPrefetching(
      OmniboxPresentationContext context,
      web::WebState* web_state,
      PageClassificationCallback classification_callback);

  // Unregisters a single WebState for prefetching.
  void UnregisterWebStateForPrefetching(web::WebState* web_state);

  base::WeakPtr<AutocompleteService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Creates a new AutocompleteController.
  std::unique_ptr<AutocompleteController> CreateAutocompleteController();
  // Creates a new OmniboxShortcutsHelper for the given context.
  std::unique_ptr<OmniboxShortcutsHelper> CreateOmniboxShortcutsHelper();

  base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
      client_factory_;
  raw_ptr<ShortcutsBackend> shortcuts_backend_;

  std::map<OmniboxPresentationContext, std::unique_ptr<AutocompleteController>>
      controllers_;
  std::map<OmniboxPresentationContext, std::unique_ptr<OmniboxShortcutsHelper>>
      shortcuts_helpers_;
  std::map<WebStateList*, ZeroSuggestPrefetcher*> web_state_list_prefetchers_;
  std::map<web::WebState*, ZeroSuggestPrefetcher*> web_state_prefetchers_;

  base::WeakPtrFactory<AutocompleteService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_
