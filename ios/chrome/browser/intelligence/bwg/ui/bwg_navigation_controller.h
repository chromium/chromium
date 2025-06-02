// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class BWGNavigationController;
@protocol BWGConsentMutator;

// Delegate for `BWGNavigationController`.
@protocol BWGNavigationControllerDelegate <NSObject>

// Informs the delegate that promo was dismissed.
- (void)promoWasDismissed:(BWGNavigationController*)navigationController;

@end

// UINavigationController that owns BWGPromo and BWGConsent view controllers.
@interface BWGNavigationController : UINavigationController

// Initializes the navigation controller. If `showPromo` is YES, the navigation
// controller will display the promo. If NO, the navigation controller will
// display the consent view.
- (instancetype)initWithPromo:(BOOL)showPromo;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<BWGConsentMutator> mutator;

// The delegate for this view controller to communicate to `BWGCoordinator`.
@property(nonatomic, weak) id<BWGNavigationControllerDelegate>
    BWGNavigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_NAVIGATION_CONTROLLER_H_
