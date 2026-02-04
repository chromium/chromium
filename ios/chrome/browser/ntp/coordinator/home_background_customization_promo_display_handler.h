// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_COORDINATOR_HOME_BACKGROUND_CUSTOMIZATION_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_NTP_COORDINATOR_HOME_BACKGROUND_CUSTOMIZATION_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

// Handler for displaying Home Background Customization Promos. This handler
// is called by the Promos Manager when the user becomes eligible for the promo.
@interface HomeBackgroundCustomizationPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_NTP_COORDINATOR_HOME_BACKGROUND_CUSTOMIZATION_PROMO_DISPLAY_HANDLER_H_
