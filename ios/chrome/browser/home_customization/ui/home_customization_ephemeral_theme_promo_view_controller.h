// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/home_customization/ui/home_customization_ephemeral_theme_promo_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// View controller for the ephemeral theme promo bottom sheet.
@interface HomeCustomizationEphemeralThemePromoViewController
    : ConfirmationAlertViewController <
          HomeCustomizationEphemeralThemePromoConsumer>

// Initializes the view controller with default button stack configuration.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_EPHEMERAL_THEME_PROMO_VIEW_CONTROLLER_H_
