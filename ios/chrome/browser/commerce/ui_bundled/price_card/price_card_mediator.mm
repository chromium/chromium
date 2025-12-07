// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_mediator.h"

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/web/public/web_state.h"

void CreatePriceCardItem(web::WebState* web_state,
                         base::OnceCallback<void(PriceCardItem*)> callback) {
  if (!web_state) {
    std::move(callback).Run(nil);
    return;
  }
  ShoppingPersistedDataTabHelper* shoppingHelper =
      ShoppingPersistedDataTabHelper::FromWebState(web_state);
  if (!shoppingHelper) {
    return;
  }
  shoppingHelper->GetPriceDrop(base::BindOnce(
      [](base::OnceCallback<void(PriceCardItem*)> callback,
         std::optional<ShoppingPersistedDataTabHelper::PriceDrop> price_drop) {
        if (!price_drop.has_value() || !price_drop->current_price ||
            !price_drop->previous_price) {
          std::move(callback).Run(nil);
          return;
        }
        PriceCardItem* price_card_item =
            [[PriceCardItem alloc] initWithPrice:price_drop->current_price
                                   previousPrice:price_drop->previous_price];
        std::move(callback).Run(price_card_item);
      },
      std::move(callback)));
}

@interface PriceCardMediator ()
@property(nonatomic, assign) WebStateList* webStateList;
@end

@implementation PriceCardMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  _webStateList = webStateList;
  return self;
}

- (void)logMetrics:(PriceDropLogId)priceDropLogId {
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    ShoppingPersistedDataTabHelper* shoppingHelper =
        ShoppingPersistedDataTabHelper::FromWebState(webState);
    if (!shoppingHelper) {
      continue;
    }
    shoppingHelper->LogMetrics(priceDropLogId);
  }
}

#pragma mark - PriceCardDataSource

- (void)priceCardForIdentifier:(web::WebStateID)identifier
                    completion:(void (^)(PriceCardItem*))completion {
  web::WebState* webState = GetWebState(
      self.webStateList, WebStateSearchCriteria{.identifier = identifier});
  CreatePriceCardItem(webState,
                      base::BindOnce(^(PriceCardItem* price_card_item) {
                        completion(price_card_item);
                      }));
}

@end
