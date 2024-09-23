// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_MEDIATOR_H_

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_view.h"

#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_data_source.h"

namespace web {
class WebState;
}  // namespace web

// Return data to power PriceCardView for a given WebState
PriceCardItem* CreatePriceCardItem(web::WebState* web_state);
// Return a WebState for a tab identifier, given the WebStateList
web::WebState* GetWebState(WebStateList* webStateList, NSString* tab_id);

@interface PriceCardMediator : NSObject <PriceCardDataSource>
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Log metrics associated with the price drop feature.
- (void)logMetrics:(PriceDropLogId)priceDropLogId;
@end

#endif  // IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_MEDIATOR_H_
