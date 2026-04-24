// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_virtual_card_cache.h"

#import "ios/web/public/navigation/navigation_context.h"

ManualFillVirtualCardCache::ManualFillVirtualCardCache(
    web::WebState* web_state) {
  web_state->AddObserver(this);
}

ManualFillVirtualCardCache::~ManualFillVirtualCardCache() = default;

void ManualFillVirtualCardCache::CacheUnmaskedCard(
    const autofill::CreditCard& card) {
  server_id_to_unmasked_card_map_[card.server_id()] = card;
}

const autofill::CreditCard* ManualFillVirtualCardCache::GetUnmaskedCard(
    const std::string& server_id) const {
  auto it = server_id_to_unmasked_card_map_.find(server_id);
  if (it != server_id_to_unmasked_card_map_.end()) {
    return &it->second;
  }
  return nullptr;
}

void ManualFillVirtualCardCache::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Clear the sensitive cache whenever the user navigates to a new document.
  if (!navigation_context->IsSameDocument()) {
    server_id_to_unmasked_card_map_.clear();
  }
}

void ManualFillVirtualCardCache::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
