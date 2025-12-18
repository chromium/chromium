// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_RATING_COORDINATOR_APP_STORE_RATING_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_APP_STORE_RATING_COORDINATOR_APP_STORE_RATING_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

// Handler for displaying the App Store Rating Promo.
// This handler is called by the Promos Manager and presents the App Store
// Rating iOS prompt to eligible users. See the App Store Rating scene agent for
// eligibility conditions.
@interface AppStoreRatingDisplayHandler : NSObject <StandardPromoDisplayHandler>

#pragma mark - PromoProtocol

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_APP_STORE_RATING_COORDINATOR_APP_STORE_RATING_DISPLAY_HANDLER_H_
