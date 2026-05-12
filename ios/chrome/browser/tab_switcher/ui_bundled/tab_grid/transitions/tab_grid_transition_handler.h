// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/ios/block_types.h"

@protocol TabGridTransitionContextProvider;

// TabGrid transitions directions available.
enum class TabGridTransitionDirection {
  kFromTabGridToBrowser,
  kFromBrowserToTabGrid,
};

@class LayoutGuideCenter;
@class TabGridTransitionHandler;
@protocol TabGridTransitionLayoutProviding;

// Parameters for the initialization of the transition handler.
struct TabGridTransitionHandlerInitParams {
  TabGridTransitionDirection direction;
  UIViewController<TabGridTransitionContextProvider>*
      browser_layout_view_controller;
  UIViewController* tab_grid_view_controller;
  UIViewController* parent_view_controller;
  // The view associated with the AppContent named guide.
  UIView* app_content_view;

  TabGridTransitionHandlerInitParams(
      TabGridTransitionDirection direction,
      UIViewController<TabGridTransitionContextProvider>*
          browser_layout_view_controller,
      UIViewController* tab_grid_view_controller,
      UIViewController* parent_view_controller,
      UIView* app_content_view)
      : direction(direction),
        browser_layout_view_controller(browser_layout_view_controller),
        tab_grid_view_controller(tab_grid_view_controller),
        parent_view_controller(parent_view_controller),
        app_content_view(app_content_view) {}

  TabGridTransitionHandlerInitParams() = delete;
};

// Handler for the transitions between the TabGrid and the Browser.
@interface TabGridTransitionHandler : NSObject

// Creates a transition handler with full animations.
- (instancetype)initWithCommonParams:
                    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params
     tabGridTransitionLayoutProvider:
         (id<TabGridTransitionLayoutProviding>)tabGridTransitionLayoutProvider
            browserLayoutGuideCenter:
                (LayoutGuideCenter*)browserLayoutGuideCenter
                 isRegularBrowserNTP:(BOOL)isRegularBrowserNTP
                           incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

// Creates a transition handler with disabled animations (Reduced Motion).
- (instancetype)initWithReducedMotionCommonParams:
    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params
    NS_DESIGNATED_INITIALIZER;

// Creates a transition handler with no animations.
- (instancetype)initWithNoAnimationCommonParams:
    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Performs the transition with a `completion` handler.
- (void)performTransitionWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_TAB_GRID_TRANSITION_HANDLER_H_
