// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/standard_promo_display_handler.h"

@interface FullscreenSigninPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

#pragma mark - PromoProtocol

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_PROMO_FULLSCREEN_SIGNIN_PROMO_DISPLAY_HANDLER_H_
