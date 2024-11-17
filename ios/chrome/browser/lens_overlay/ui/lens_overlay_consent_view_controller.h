// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@class LensOverlayConsentViewController;

// Delegate for LensOverlayConsentViewController.
@protocol
    LensOverlayConsentViewControllerDelegate <PromoStyleViewControllerDelegate>

/// Called when the user asks to learn more about lens.
- (void)didPressLearnMore;

@end

/// View controller that contains the ToS and prompts the user for acceptance.
/// Relies on the `delegate` to actually set the pref.
@interface LensOverlayConsentViewController : PromoStyleViewController

// The delegate to invoke when buttons are tapped.
@property(nonatomic, weak) id<LensOverlayConsentViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_VIEW_CONTROLLER_H_
