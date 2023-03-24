// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_PROMO_WHATS_NEW_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_PROMO_WHATS_NEW_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

class PromosManager;

// Handler for displaying What's New. This handler is called by the Promos
// Manager once on the 6th day or 6th launch after the FRE.
@interface WhatsNewPromoDisplayHandler : NSObject <StandardPromoDisplayHandler>

- (instancetype)init NS_UNAVAILABLE;

// Promos Manager to alert if the user uses What's New.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
    NS_DESIGNATED_INITIALIZER;

#pragma mark - PromoProtocol

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_PROMO_WHATS_NEW_PROMO_DISPLAY_HANDLER_H_
