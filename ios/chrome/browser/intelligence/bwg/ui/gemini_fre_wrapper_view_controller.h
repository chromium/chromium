// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@class GeminiConsentConfiguration;
@protocol GeminiFirstRunMutator;

// UIViewController that owns GeminiPromo and GeminiConsent view controllers and
// manages their transitions with a horizontal scroll view.
//
// TODO(crbug.com/519213385): Rename to GeminiFirstRunWrapperViewController.
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

// Initializes the view controller.
// If `showPromo` is YES, the view controller will display the promo.
// If NO, the view controller will display the consent view.
// `freType` specifies the type of Gemini FRE being shown.
// `consentConfiguration` provides the configuration for the consent view.
- (instancetype)initWithPromo:(BOOL)showPromo
                      FREType:(GeminiFREType)FREType
         consentConfiguration:(GeminiConsentConfiguration*)consentConfiguration
    NS_DESIGNATED_INITIALIZER;

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
@property(nonatomic, weak) id<GeminiFirstRunMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FRE_WRAPPER_VIEW_CONTROLLER_H_
