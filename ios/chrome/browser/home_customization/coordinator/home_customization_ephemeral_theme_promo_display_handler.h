// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

@protocol EphemeralThemePromoCommands;

// Handler for displaying Home Customization Ephemeral Theme Promos. This
// handler is called by the Promos Manager when the user becomes eligible for
// the promo.
@interface HomeCustomizationEphemeralThemePromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// Initializes the handler with `ephemeralThemePromoHandler`.
- (instancetype)initWithEphemeralThemePromoHandler:
    (id<EphemeralThemePromoCommands>)ephemeralThemePromoHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_DISPLAY_HANDLER_H_
