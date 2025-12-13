// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/omnibox_shortcuts_helper.h"

#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ui/base/page_transition_types.h"

OmniboxShortcutsHelper::OmniboxShortcutsHelper(
    ShortcutsBackend* shortcuts_backend)
    : shortcuts_backend_(shortcuts_backend) {
  // shorcuts_backend is not available in incognito.
}

OmniboxShortcutsHelper::~OmniboxShortcutsHelper() {
  scoped_observations_.RemoveAllObservations();
  web_state_tracker_.clear();
}

void OmniboxShortcutsHelper::OnAutocompleteAccept(
    const std::u16string& text,
    const AutocompleteMatch& match,
    web::WebState* web_state) {
  if (web_state && shortcuts_backend_) {
    const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
    if (web_state_tracker_.find(web_state_id) == web_state_tracker_.end()) {
      scoped_observations_.AddObservation(web_state);
    }
    const ShortcutElement shortcutElement{text, match};
    web_state_tracker_.insert_or_assign(web_state_id, shortcutElement);
  }
}

void OmniboxShortcutsHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  auto it = web_state_tracker_.find(web_state_id);
  if (it == web_state_tracker_.end()) {
    return;
  }

  ShortcutElement shortcut = it->second;
  web_state_tracker_.erase(it);
  scoped_observations_.RemoveObservation(web_state);

  // Add the shortcut if the navigation from the omnibox was successful.
  if (!navigation_context->GetError() && shortcuts_backend_ &&
      (navigation_context->GetPageTransition() &
       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    shortcuts_backend_->AddOrUpdateShortcut(shortcut.text, shortcut.match);
  }
}

void OmniboxShortcutsHelper::WebStateDestroyed(web::WebState* web_state) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  web_state_tracker_.erase(web_state_id);
  scoped_observations_.RemoveObservation(web_state);
}
