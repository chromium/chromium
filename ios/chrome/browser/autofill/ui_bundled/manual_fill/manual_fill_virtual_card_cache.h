// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_

#import <map>
#import <string>

#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// A tab-scoped cache for unmasked virtual cards.
class ManualFillVirtualCardCache
    : public web::WebStateUserData<ManualFillVirtualCardCache>,
      public web::WebStateObserver {
 public:
  ~ManualFillVirtualCardCache() override;

  void CacheUnmaskedCard(const autofill::CreditCard& card);
  const autofill::CreditCard* GetUnmaskedCard(const std::string& guid) const;

 private:
  friend class web::WebStateUserData<ManualFillVirtualCardCache>;
  explicit ManualFillVirtualCardCache(web::WebState* web_state);

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  std::map<std::string, autofill::CreditCard> guid_to_unmasked_card_map_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_VIRTUAL_CARD_CACHE_H_
