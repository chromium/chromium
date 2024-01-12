// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Base delegate protocol for the base promo style controller to
// communicate with screen-specific coordinators. Only the shared action buttons
// are included in this base protocol; screens with additional buttons should
// extend this protocol.
@protocol PromoStyleViewControllerDelegate <NSObject>

@optional

// Invoked when the primary action button is tapped.
- (void)didTapPrimaryActionButton;

// Invoked when the secondary action button is tapped.
- (void)didTapSecondaryActionButton;

// Invoked when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton;

// Invoked when the top left question mark button is tapped.
- (void)didTapLearnMoreButton;

// Invoked when a link in the disclaimer is tapped.
- (void)didTapURLInDisclaimer:(NSURL*)URL;

// Invoked when the view controller has been dismissed.
- (void)didDismissViewController;

@end

#endif  // IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_DELEGATE_H_
