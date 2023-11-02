// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_

// The 3 stages or steps of the transitions handled by the view revealing
// vertical pan handler class.
enum class ViewRevealState {
  Hidden,    // The view is not revealed.
  Peeked,    // The view is only partially revealed.
  Revealed,  // The view is mostly revealed.
};

enum class ViewRevealTrigger {
  Unknown,           // Unknown, only during tests.
  TabStrip,          // User is dragging from the tab strip.
  TabGrid,           // User requested tab grid opening or closing or new tab.
  PrimaryToolbar,    // User is dragging from the primary toolbar.
  FakeTab,           // User tapped the fake tab at bottom.
  Fullscreen,        // Page change caused entering or exiting fullscreen.
  UserNavigation,    // Triggered by user web page navigation.
  WebScroll,         // Triggered by user scrolling the web view.
  AppBackgrounding,  // Triggered by app being backgrounded.
  OmniboxFocus,      // Triggered by user focus on omnibox.
  BackgroundTap,     // Triggered by user tap on background.
  BackgroundSwipe,   // Triggered by user swipping up background.
};

// Protocol defining an interface to handle animations from the view revealing
// pan gesture handler.
@protocol ViewRevealingAnimatee <NSObject>

@optional

// Called before a view reveal animation. Takes as argument both the state in
// which the view revealer is before the animation and the state that the view
// revealer will transition to.
- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState;

// Called inside an animation block to animate the revealing of the view. Takes
// as argument the state in which the view revealer will be after the animation.
- (void)animateViewReveal:(ViewRevealState)nextViewRevealState;

// Called inside the completion block of a view reveal animation. Takes as
// argument the state in which the view revealer is now.
- (void)didAnimateViewRevealFromState:(ViewRevealState)startViewRevealState
                              toState:(ViewRevealState)currentViewRevealState
                              trigger:(ViewRevealTrigger)trigger;

// Tells the animatee when a web view drag starts and when it ends (at the end
// of deceleration).
- (void)webViewIsDragging:(BOOL)dragging
          viewRevealState:(ViewRevealState)viewRevealState;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_
