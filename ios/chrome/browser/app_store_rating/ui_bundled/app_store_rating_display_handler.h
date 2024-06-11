// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

// Handler for displaying the App Store Rating Promo.
//
// This handler is called by the Promos Manager and presents the App Store
// Rating promo to eligible users. Users are considered eligible if they
// 1) have used Chrome for at least 15 total days,
// 2) have used Chrome for at least 3 days in the past 7 days,
// 3) have enabled CPE, and
// 4) have Chrome set as their default browser.
@interface AppStoreRatingDisplayHandler : NSObject <StandardPromoDisplayHandler>

#pragma mark - PromoProtocol

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_APP_STORE_RATING_UI_BUNDLED_APP_STORE_RATING_DISPLAY_HANDLER_H_
