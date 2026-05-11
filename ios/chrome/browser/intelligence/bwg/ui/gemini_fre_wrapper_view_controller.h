// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@protocol GeminiConsentMutator;

// UIViewController that owns GeminiPromo and GeminiConsent view controllers and
// manages their transitions with a horizontal scroll view.
//
// The layout is structured as follows:
// +--------------------------------+
// |      Vertical Scroll View      |
// |  +--------------------------+  |
// |  |          Logo            |  |
// |  +--------------------------+  |
// |  +--------------------------+  |
// |  |  Horizontal Scroll View  |  |
// |  | +---------+  +---------+ |  |
// |  | |  Promo  |->| Consent | |  |
// |  | +---------+  +---------+ |  |
// |  +--------------------------+  |
// +--------------------------------+
// |         Buttons stack          |
// +--------------------------------+
@interface GeminiFREWrapperViewController : ButtonStackViewController

// Initializes the navigation controller.
// If `showPromo` is YES, the navigation controller will display the promo.
// If NO, the navigation controller will display the consent view.
// `isAccountManaged` indicates whether the account is managed.
// `freType` specifies the type of Gemini FRE being shown.
- (instancetype)initWithPromo:(BOOL)showPromo
             isAccountManaged:(BOOL)isAccountManaged
        useStrictLegalConsent:(BOOL)useStrictLegalConsent
                      FREType:(GeminiFREType)FREType
                      country:(NSString*)country NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
    NS_UNAVAILABLE;
- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass
                              toolbarClass:(Class)toolbarClass NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GeminiConsentMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_
