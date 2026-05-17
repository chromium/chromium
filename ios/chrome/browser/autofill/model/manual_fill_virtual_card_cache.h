// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_

#import <map>
#import <optional>
#import <string>

#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/origin.h"

// A tab-scoped cache for unmasked virtual cards.
class ManualFillVirtualCardCache
    : public web::WebStateUserData<ManualFillVirtualCardCache>,
      public web::WebStateObserver {
 public:
  ~ManualFillVirtualCardCache() override;

  // Caches the unmasked card, associated with the origin where it was unmasked.
  void CacheUnmaskedCard(const autofill::CreditCard& card,
                         const url::Origin& origin);

  // Sets the origin of the frame that initiated the active unmasking request.
  void SetUnmaskingOrigin(const url::Origin& origin);

  // Returns the stored unmasking origin, and clears it.
  url::Origin GetUnmaskingOrigin();

  // Returns the cached card only if the server_id matches and the current
  // origin matches the origin where the card was unmasked.
  const autofill::CreditCard* GetUnmaskedCard(
      const std::string& server_id,
      const url::Origin& current_origin) const;

 private:
  friend class web::WebStateUserData<ManualFillVirtualCardCache>;
  explicit ManualFillVirtualCardCache(web::WebState* web_state);

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  struct CachedCard {
    autofill::CreditCard card;
    url::Origin origin;
  };

  std::map<std::string, CachedCard> server_id_to_unmasked_card_map_;

  // The origin of the frame that initiated the active unmasking request.
  std::optional<url::Origin> unmasking_origin_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_
