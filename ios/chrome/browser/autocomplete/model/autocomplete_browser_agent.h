// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_BROWSER_AGENT_H_

#import <map>
#import <memory>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/autocomplete/model/omnibox_shortcuts_helper.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"

@class ZeroSuggestPrefetcher;
class AutocompleteController;
class Browser;

/// Keyed Service that owns and manages long-lived Omnibox objects
/// (AutocompleteController, etc.) per presentation context.
class AutocompleteBrowserAgent
    : public BrowserUserData<AutocompleteBrowserAgent> {
 public:
  AutocompleteBrowserAgent(const AutocompleteBrowserAgent&) = delete;
  AutocompleteBrowserAgent& operator=(const AutocompleteBrowserAgent&) = delete;
  ~AutocompleteBrowserAgent() override;

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

  base::WeakPtr<AutocompleteBrowserAgent> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class BrowserUserData<AutocompleteBrowserAgent>;

  explicit AutocompleteBrowserAgent(Browser* browser);

  // Creates a new AutocompleteController.
  std::unique_ptr<AutocompleteController> CreateAutocompleteController();
  // Creates a new OmniboxShortcutsHelper for the given context.
  std::unique_ptr<OmniboxShortcutsHelper> CreateOmniboxShortcutsHelper();

  std::map<OmniboxPresentationContext, std::unique_ptr<AutocompleteController>>
      controllers_;
  std::map<OmniboxPresentationContext, std::unique_ptr<OmniboxShortcutsHelper>>
      shortcuts_helpers_;
  std::map<WebStateList*, ZeroSuggestPrefetcher*> web_state_list_prefetchers_;
  std::map<web::WebState*, ZeroSuggestPrefetcher*> web_state_prefetchers_;

  base::WeakPtrFactory<AutocompleteBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_BROWSER_AGENT_H_
