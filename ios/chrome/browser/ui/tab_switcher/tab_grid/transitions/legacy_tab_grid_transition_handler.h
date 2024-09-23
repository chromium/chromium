// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@protocol LegacyGridTransitionAnimationLayoutProviding;

// Handler for the transitions between the TabGrid and the Browser.
@interface LegacyTabGridTransitionHandler : NSObject

- (instancetype)initWithLayoutProvider:
    (id<LegacyGridTransitionAnimationLayoutProviding>)layoutProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Whether the animations should be disabled.
@property(nonatomic, assign) BOOL animationDisabled;

// Starts the transition from the `browser` to the `tabGrid`. Assumes that the
// `browser` is currently a child ViewController of the `tabGrid`. The active
// page of the `tabGrid` for the transition is `activePage`. `toTabGroup`
// represents if the TabGrid should open a TabGroup. Calls `completion` when the
// transition finishes.
- (void)transitionFromBrowser:(UIViewController*)browser
                    toTabGrid:(UIViewController*)tabGrid
                   toTabGroup:(BOOL)toTabGroup
                   activePage:(TabGridPage)activePage
               withCompletion:(void (^)(void))completion;

// Starts the transition from `tabGrid` to `browser`. Adds `browser` as a child
// ViewController of `tabGrid`, covering it. The active page of the `tabGrid`
// for the transition is `activePage`. Calls `completion` when the transition
// finishes.
- (void)transitionFromTabGrid:(UIViewController*)tabGrid
                    toBrowser:(UIViewController*)browser
                   activePage:(TabGridPage)activePage
               withCompletion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TRANSITIONS_LEGACY_TAB_GRID_TRANSITION_HANDLER_H_
