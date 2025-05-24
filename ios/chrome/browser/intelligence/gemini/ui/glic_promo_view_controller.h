// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol GLICConsentMutator;
@protocol GLICPromoViewControllerDelegate;

// GLIC consent View Controller.
@interface GLICPromoViewController : PromoStyleViewController

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GLICConsentMutator> mutator;
// The delegate for this view controller to communicate to the GLIC
// NavigationController.
@property(nonatomic, weak) id<GLICPromoViewControllerDelegate>
    glicPromoDelegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_PROMO_VIEW_CONTROLLER_H_
