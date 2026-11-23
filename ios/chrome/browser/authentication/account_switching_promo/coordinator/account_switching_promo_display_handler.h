// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_COORDINATOR_ACCOUNT_SWITCHING_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_COORDINATOR_ACCOUNT_SWITCHING_PROMO_DISPLAY_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

NS_ASSUME_NONNULL_BEGIN

@protocol PromosManagerCommands;

// Handler for displaying the smart account switching full-screen startup promo.
@interface AccountSwitchingPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// `PromosManagerCommands` handler used to display the promo.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_SWITCHING_PROMO_COORDINATOR_ACCOUNT_SWITCHING_PROMO_DISPLAY_HANDLER_H_
