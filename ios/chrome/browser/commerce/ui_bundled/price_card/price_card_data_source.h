// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_item.h"

namespace web {
class WebStateID;
}  // namespace web

@protocol PriceCardDataSource
// Returns data which powers PriceCardView given a tab identifier. The
// PriceCardView will be displayed on top of the screenshot in the Tab
// Switching UI for the corresponding Tab.
- (void)priceCardForIdentifier:(web::WebStateID)identifier
                    completion:(void (^)(PriceCardItem*))completion;
@end

#endif  // IOS_CHROME_BROWSER_COMMERCE_UI_BUNDLED_PRICE_CARD_PRICE_CARD_DATA_SOURCE_H_
