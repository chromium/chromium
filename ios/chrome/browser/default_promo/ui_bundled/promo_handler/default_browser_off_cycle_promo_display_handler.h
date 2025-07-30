// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_OFF_CYCLE_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_OFF_CYCLE_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/standard_promo_display_handler.h"

// Handler for displaying Default Browser off-cycle Promos. This handler
// is called by the Promos Manager when the user becomes eligible for the
// off-cycle promo.
@interface DefaultBrowserOffCyclePromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_OFF_CYCLE_PROMO_DISPLAY_HANDLER_H_
