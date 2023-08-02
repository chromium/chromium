// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commerce/price_card/price_card_mediator.h"

#import "ios/chrome/browser/ui/commerce/price_card/price_card_item.h"
#import "ios/web/public/web_state.h"

PriceCardItem* CreatePriceCardItem(web::WebState* web_state) {
  if (!web_state)
    return nil;
  ShoppingPersistedDataTabHelper* shoppingHelper =
      ShoppingPersistedDataTabHelper::FromWebState(web_state);
  if (!shoppingHelper || !shoppingHelper->GetPriceDrop() ||
      !shoppingHelper->GetPriceDrop()->current_price ||
      !shoppingHelper->GetPriceDrop()->previous_price)
    return nil;
  return [[PriceCardItem alloc]
      initWithPrice:shoppingHelper->GetPriceDrop()->current_price
      previousPrice:shoppingHelper->GetPriceDrop()->previous_price];
}

web::WebState* GetWebState(WebStateList* web_state_list, NSString* tab_id) {
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if ([tab_id isEqualToString:web_state->GetStableIdentifier()])
      return web_state;
  }
  return nullptr;
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
    if (!shoppingHelper)
      continue;
    shoppingHelper->LogMetrics(priceDropLogId);
  }
}

#pragma mark - PriceCardDataSource

- (void)priceCardForIdentifier:(NSString*)identifier
                    completion:(void (^)(PriceCardItem*))completion {
  web::WebState* webState = GetWebState(self.webStateList, identifier);
  completion(CreatePriceCardItem(webState));
}
@end
