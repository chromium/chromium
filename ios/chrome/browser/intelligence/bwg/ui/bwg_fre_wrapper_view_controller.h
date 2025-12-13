// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_WRAPPER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BWGConsentMutator;

// UIViewController that owns BWGPromo and BWGConsent view controllers and
// manages their transitions.
@interface BWGFREWrapperViewController : UIViewController

// Initializes the navigation controller.
// If `showPromo` is YES, the navigation controller will display the promo.
// If NO, the navigation controller will display the consent view.
// `isAccountManaged` indicates whether the account is managed.
- (instancetype)initWithPromo:(BOOL)showPromo
             isAccountManaged:(BOOL)isAccountManaged NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
    NS_UNAVAILABLE;
- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass
                              toolbarClass:(Class)toolbarClass NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<BWGConsentMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_FRE_WRAPPER_VIEW_CONTROLLER_H_
