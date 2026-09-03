// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_view_controller.h"

@class BackendPromoCustomUIParams;

// View controller for presenting backend promotional custom UI modals with
// dynamic Lottie animations and dynamic theme color mappings.
@interface BackendPromoCustomUIViewController : AnimatedPromoViewController

// Instantiates and returns the appropriate view controller based on `params`:
// - Returns BackendPromoCustomUIViewController if a Lottie animation resource
//   matching `params.imageURL` exists in the main bundle.
// - Returns ConfirmationAlertViewController if no Lottie resource exists.
+ (UIViewController*)
    viewControllerWithParams:(BackendPromoCustomUIParams*)params
               actionHandler:(id<ConfirmationAlertActionHandler>)actionHandler;

// Initializes the view controller with the specified custom UI parameters.
- (instancetype)initWithParams:(BackendPromoCustomUIParams*)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_VIEW_CONTROLLER_H_
