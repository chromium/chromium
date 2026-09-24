// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_FIRST_RUN_UI_GEMINI_FIRST_RUN_WRAPPER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_FIRST_RUN_UI_GEMINI_FIRST_RUN_WRAPPER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@class GeminiConsentConfiguration;
@protocol GeminiFirstRunMutator;

// UIViewController that owns GeminiPromo and GeminiConsent view controllers and
// manages their transitions with a horizontal scroll view.
//
// The layout is structured as follows:
// +--------------------------------+
// :      Vertical Scroll View      :
// :  +--------------------------+  :
// :  :          Logo            :  :
// :  +--------------------------+  :
// :  +--------------------------+  :
// :  :  Horizontal Scroll View  :  :
// :  : +---------+  +---------+ :  :
// :  : :  Promo  :->: Consent : :  :
// :  : +---------+  +---------+ :  :
// :  +--------------------------+  :
// +--------------------------------+
// :         Buttons stack          :
// +--------------------------------+
@interface GeminiFirstRunWrapperViewController : ButtonStackViewController

// Initializes the view controller.
// If `showPromo` is YES, the view controller will display the promo.
// If NO, the view controller will display the consent view.
// `firstRunType` specifies the type of Gemini First Run being shown.
// `consentConfiguration` provides the configuration for the consent view.
- (instancetype)initWithPromo:(BOOL)showPromo
                 firstRunType:(GeminiFirstRunType)firstRunType
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

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_FIRST_RUN_UI_GEMINI_FIRST_RUN_WRAPPER_VIEW_CONTROLLER_H_
