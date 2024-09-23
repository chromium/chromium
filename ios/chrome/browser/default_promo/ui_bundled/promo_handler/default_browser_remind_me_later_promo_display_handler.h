// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_REMIND_ME_LATER_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_REMIND_ME_LATER_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

// Handler for displaying Default Browser Remind Me Later Promos. This handler
// is called by the Promos Manager after the main default browser promo has been
// displayed and the user asks to be reminded later.
@interface DefaultBrowserRemindMeLaterPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_PROMO_HANDLER_DEFAULT_BROWSER_REMIND_ME_LATER_PROMO_DISPLAY_HANDLER_H_
