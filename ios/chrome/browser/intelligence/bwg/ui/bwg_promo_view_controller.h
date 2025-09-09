// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_view_controller_protocol.h"

@protocol BWGConsentMutator;
@protocol BWGPromoViewControllerDelegate;

// BWG consent View Controller.
@interface BWGPromoViewController
    : UIViewController <BWGFREViewControllerProtocol>

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<BWGConsentMutator> mutator;
// The delegate for this view controller to communicate to the BWG
// NavigationController.
@property(nonatomic, weak) id<BWGPromoViewControllerDelegate> BWGPromoDelegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_PROMO_VIEW_CONTROLLER_H_
