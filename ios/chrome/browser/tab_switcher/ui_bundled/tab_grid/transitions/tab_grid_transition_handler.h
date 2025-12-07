// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// TabGrid transitions directions available.
enum class TabGridTransitionDirection {
  kFromTabGridToBrowser,
  kFromBrowserToTabGrid,
};

// Transition types available.
enum class TabGridTransitionType {
  kNormal,
  kReducedMotion,
  kAnimationDisabled,
};

@class LayoutGuideCenter;
@class TabGridTransitionHandler;
@protocol TabGridTransitionLayoutProviding;

// Handler for the transitions between the TabGrid and the Browser.
@interface TabGridTransitionHandler : NSObject

// Creates the transition object based on the provided `transitionType`,
// `direction`, `tabGridTransitionLayoutProvider`, `tabGridViewController`,
// `bvcContainerViewController`, `layoutGuideCenter`, `isRegularBrowserNTP`,
// and `isIncognito`.
- (instancetype)initWithTransitionType:(TabGridTransitionType)transitionType
                             direction:(TabGridTransitionDirection)direction
       tabGridTransitionLayoutProvider:
           (id<TabGridTransitionLayoutProviding>)tabGridTransitionLayoutProvider
                 tabGridViewController:(UIViewController*)tabGridViewController
            bvcContainerViewController:
                (UIViewController*)bvcContainerViewController
                     layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                   isRegularBrowserNTP:(BOOL)isRegularBrowserNTP
                             incognito:(BOOL)incognito
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Performs the transition with a `completion` handler.
- (void)performTransitionWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
