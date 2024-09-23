// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

// Handler for displaying Default Browser Promos. This handler is called by the
// Promos Manager once on the 7th day or 7th launch after the FRE.
@interface DefaultBrowserPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

#pragma mark - PromoProtocol

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_PROMO_DISPLAY_HANDLER_H_
