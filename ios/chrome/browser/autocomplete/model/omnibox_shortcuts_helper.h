// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_SHORTCUTS_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_SHORTCUTS_HELPER_H_

#import <string>
#import <unordered_map>

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "ios/web/public/web_state_observer.h"

class ShortcutsBackend;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

/// Helper class tracking navigations from the Omnibox to add shortcuts.
class OmniboxShortcutsHelper : public web::WebStateObserver {
 public:
  explicit OmniboxShortcutsHelper(ShortcutsBackend* shortcuts_backend);
  ~OmniboxShortcutsHelper() override;

  OmniboxShortcutsHelper(const OmniboxShortcutsHelper&) = delete;
  OmniboxShortcutsHelper& operator=(const OmniboxShortcutsHelper&) = delete;

  /// Called when a match is accepted in the Omnibox.
  void OnAutocompleteAccept(const std::u16string& text,
                            const AutocompleteMatch& match,
                            web::WebState* web_state);

 private:
  // web::WebStateObserver.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Object associated with a web state id in `web_state_tracker_`. If the
  // navigation succeeds, the shortcut is stored in the ShortcutsDatabase.
  struct ShortcutElement {
    std::u16string text;
    AutocompleteMatch match;
  };

  raw_ptr<ShortcutsBackend> shortcuts_backend_;

  // Stores observed navigations from the omnibox. Items are removed once
  // navigation finishes or when it's destroyed.
  std::unordered_map<int32_t, ShortcutElement> web_state_tracker_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      scoped_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_OMNIBOX_SHORTCUTS_HELPER_H_
