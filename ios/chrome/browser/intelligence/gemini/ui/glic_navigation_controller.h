// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol GLICConsentMutator;

// UINavigationController that owns GLICPromo and GLICConsent view controllers.
@interface GLICNavigationController : UINavigationController

// Initializes the navigation controller. If `showPromo` is YES, the navigation
// controller will display the promo. If NO, the navigation controller will
// display the consent view.
- (instancetype)initWithPromo:(BOOL)showPromo;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GLICConsentMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_NAVIGATION_CONTROLLER_H_
