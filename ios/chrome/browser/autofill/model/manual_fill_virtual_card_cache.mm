// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/model/manual_fill_virtual_card_cache.h"

#import "base/check.h"
#import "ios/web/public/navigation/navigation_context.h"

ManualFillVirtualCardCache::ManualFillVirtualCardCache(
    web::WebState* web_state) {
  web_state->AddObserver(this);
}

ManualFillVirtualCardCache::~ManualFillVirtualCardCache() = default;

void ManualFillVirtualCardCache::CacheUnmaskedCard(
    const autofill::CreditCard& card,
    const url::Origin& origin) {
  server_id_to_unmasked_card_map_[card.server_id()] = {card, origin};
}

const autofill::CreditCard* ManualFillVirtualCardCache::GetUnmaskedCard(
    const std::string& server_id,
    const url::Origin& current_origin) const {
  auto it = server_id_to_unmasked_card_map_.find(server_id);
  if (it != server_id_to_unmasked_card_map_.end() &&
      it->second.origin.IsSameOriginWith(current_origin)) {
    return &it->second.card;
  }
  return nullptr;
}

void ManualFillVirtualCardCache::SetUnmaskingOrigin(const url::Origin& origin) {
  CHECK(!unmasking_origin_.has_value());
  unmasking_origin_ = origin;
}

url::Origin ManualFillVirtualCardCache::GetUnmaskingOrigin() {
  url::Origin origin = unmasking_origin_.value_or(url::Origin());
  unmasking_origin_.reset();
  return origin;
}

void ManualFillVirtualCardCache::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Clear the sensitive cache whenever the user navigates to a new document.
  if (!navigation_context->IsSameDocument()) {
    server_id_to_unmasked_card_map_.clear();
    unmasking_origin_.reset();
  }
}

void ManualFillVirtualCardCache::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}
