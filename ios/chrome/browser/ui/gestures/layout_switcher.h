// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_H_

// The 2 layout states of a view that is revealed by the view revealing vertical
// pan handler class.
enum class LayoutSwitcherState {
  Horizontal,  // The view layout when the view is a horizontal strip.
  Grid,        // The view layout when the view is a grid of elements.
};

// Interface to manage interactive animated transitions of layout.
@protocol LayoutSwitcher

// Returns current layout state.
@property(nonatomic, readonly) LayoutSwitcherState currentLayoutSwitcherState;

// Notifies of a transition of layout to the specified state. Called when the
// view revealing vertical pan handler starts a transition of layout. The
// conformer should prepare its layout for a transition to `nextState`, that
// should execute the specified completion block on completion.
- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion;

// Notifies of a change in the progress of the transition of layout currently in
// progress.
- (void)didUpdateTransitionLayoutProgress:(CGFloat)progress;

// Notifies of a transition animation that happened in the correct direction if
// `success` and in the reverse direction otherwise.
- (void)didTransitionToLayoutSuccessfully:(BOOL)success;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_H_
