// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_DISPLAY_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/ui/promos_manager/promo_protocol.h"

// StandardPromoDisplayHandler enables feature teams to completely handle
// what happens after their promo, `identifier`, is triggered for display.
//
// When the Promos Manager determines it's time to display the promo
// `identifier`, it will call `handleDisplay`.
// If this method is used to display promos, the promo must alert the Promos
// Manager when it is dismissed by calling the `PromosManagerCoordinator`'s
// `promoWasDismissed` method. the `PromosManagerUIHandler` protocol can be used
// for this purpose.
@protocol StandardPromoDisplayHandler <PromoProtocol>

@required

// WARNING: `handleDisplay` should be used with great caution and care as it
// shifts almost all of the responsibility of the promo's display, action
// handling, and metrics tracking onto feature teams. The Promos Manager will
// still determine the optimal time to call this method based on its own
// internal logic and criteria.
- (void)handleDisplay;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_STANDARD_PROMO_DISPLAY_HANDLER_H_
