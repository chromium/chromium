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
  guid_to_unmasked_card_map_[card.guid()] = card;
}

const autofill::CreditCard* ManualFillVirtualCardCache::GetUnmaskedCard(
    const std::string& guid) const {
  auto it = guid_to_unmasked_card_map_.find(guid);
  if (it != guid_to_unmasked_card_map_.end()) {
    return &it->second;
  }
  return nullptr;
}

void ManualFillVirtualCardCache::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Clear the sensitive cache whenever the user navigates to a new document.
  if (!navigation_context->IsSameDocument()) {
    guid_to_unmasked_card_map_.clear();
  }
}

void ManualFillVirtualCardCache::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
