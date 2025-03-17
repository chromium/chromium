// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_DELEGATE_H_

// The SideSwipeUIController delegate.
@protocol SideSwipeUIControllerDelegate

// Redisplays the tab view.
- (void)sideSwipeRedisplayTabView;

// View that will be animated alongside the swipe by setting its frame.
//
// If the animation is contained within the browser's content area,
// use sideSwipeContentView for the swipe animation.
//
// If the animation includes browser chrome elements (e.g., toolbars),
// use sideSwipeFullscreenView for the swipe animation.
//
// Example: Swiping between the Lens overlay UI and a normal web page
// requires sideSwipeFullscreenView because the toolbars are involved.
- (UIView*)sideSwipeContentView;
- (UIView*)sideSwipeFullscreenView;

// Controls the visibility of views such as the findbar, infobar and voice
// search bar.
- (void)updateAccessoryViewsForSideSwipeWithVisibility:(BOOL)visible;

// Returns the height of the header view for the current tab.
- (CGFloat)headerHeightForSideSwipe;

// Returns `YES` if side swipe should be blocked from initiating, such as when
// voice search is up, or if the tools menu is enabled.
- (BOOL)preventSideSwipe;

// Returns whether a swipe on the toolbar can start.
- (BOOL)canBeginToolbarSwipe;

// Returns the top toolbar's view.
- (UIView*)topToolbarView;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UI_CONTROLLER_DELEGATE_H_
