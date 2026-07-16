// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_PAGE_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@protocol GeminiFirstRunStep;

// A generic `UIViewController` container that displays a sequence of onboarding
// steps with a horizontal scroll transition.
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
// |  | |  Step 0 |->| Step 1  | |  |
// |  | +---------+  +---------+ |  |
// |  +--------------------------+  |
// +--------------------------------+
// |         Buttons stack          |
// +--------------------------------+
@interface GeminiFirstRunPageViewController : ButtonStackViewController

// The currently active step being shown.
@property(nonatomic, strong, readonly)
    UIViewController<GeminiFirstRunStep>* currentStep;

// Initializes the page view controller with an ordered list of step view
// controllers and a flag specifying whether to show the fixed branding logo at
// the top.
- (instancetype)initWithSteps:
                    (NSArray<UIViewController<GeminiFirstRunStep>*>*)steps
           showBrandingHeader:(BOOL)showBrandingHeader
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

// Transitions the layout to the next step in the `steps` array.
- (void)transitionToNextStepAnimated:(BOOL)animated;

// Transitions the layout to a specific step.
- (void)transitionToStep:(UIViewController<GeminiFirstRunStep>*)step
                animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_PAGE_VIEW_CONTROLLER_H_
