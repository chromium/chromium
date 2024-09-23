// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_direction.h"

@protocol TabGridTransitionLayoutProviding;

namespace {

// Transition types available.
enum class TabGridTransitionType {
  kNormal,
  kReducedMotion,
  kAnimationDisabled,
};

}  // namespace

@class TabGridTransitionHandler;

// Handler for the transitions between the TabGrid and the Browser.
@interface TabGridTransitionHandler : NSObject

// Creates the transition object based on the provided `transitionType`,
// `direction`, `tabGridViewController` and `bvcContainerViewController`.
- (instancetype)initWithTransitionType:(TabGridTransitionType)transitionType
                             direction:(TabGridTransitionDirection)direction
                 tabGridViewController:
                     (UIViewController<TabGridTransitionLayoutProviding>*)
                         tabGridViewController
            bvcContainerViewController:
                (UIViewController*)bvcContainerViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Performs the transition with a `completion` handler.
- (void)performTransitionWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
