// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/layout_switcher_provider.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

// Responsible for handling vertical pan gestures to reveal/hide a view behind
// another.
// When passed UIScrollViewDelegate calls, this object will handle those calls
// to reveal/hide a view with some specific behaviors:
//   1. The view will only reveal when the scrollView scroll starts from the
//      top of the scroll view.
//   2. When the view is peeked, scrolling down (moving the finger up) will
//      cause the view to become hidden.
// TODO(crbug.com/1123512): Add support for going straight from a Hidden state
// to a revealed state (and vice-versa) if the gesture's translation and
// velocity are enough to trigger such transition.
@interface ViewRevealingVerticalPanHandler
    : NSObject <CRWWebViewScrollViewProxyObserver,
                UIGestureRecognizerDelegate,
                UIScrollViewDelegate>

// `peekedHeight` is the height of the view when peeked (partially revealed).
// `baseViewHeight` is the height of the base view.
- (instancetype)initWithPeekedHeight:(CGFloat)peekedHeight
                      baseViewHeight:(CGFloat)baseViewHeight
                        initialState:(ViewRevealState)initialState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// When creating a pan gesture, this method is passed as the action on the init
// method. It is called when a gesture starts, moves, or ends.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture;

// Adds UI element to the set of animated objects. The set holds strong
// references to the animatees. It gets cleaned up at destruction of the
// handler.
- (void)addAnimatee:(id<ViewRevealingAnimatee>)animatee;

// Requests the pan handler to transition to `state`. Depending on the
// internals, this may not happen immediately.
- (void)setNextState:(ViewRevealState)state
            animated:(BOOL)animated
             trigger:(ViewRevealTrigger)trigger;

// Height of the view that will be revealed after the transition to Peeked
// state.
@property(nonatomic, assign, readonly) CGFloat peekedHeight;

// Height of the base view. It changes when the user rotates the screen.
@property(nonatomic, assign) CGFloat baseViewHeight;

// The provider for the object that switches the layout of the revealed view
// from horizontal (Peeked state) to full (Revealed state).
@property(nonatomic, weak) id<LayoutSwitcherProvider> layoutSwitcherProvider;

// Represents one of the three possible "states" of view reveal, which are:
// No view revealed (Hidden), view partially revealed (Peeked), and view
// completely revealed (Revealed).
@property(nonatomic, readonly) ViewRevealState currentState;

@end

@interface ViewRevealingPanGestureRecognizer : UIPanGestureRecognizer

// Inits a custom `UIPanGestureRecognizer` for the given `trigger`.
- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
                       trigger:(ViewRevealTrigger)trigger;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
