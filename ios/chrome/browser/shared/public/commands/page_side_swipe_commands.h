// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_SIDE_SWIPE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_SIDE_SWIPE_COMMANDS_H_

#import <UIKit/UIKit.h>

@protocol PageSideSwipeCommands

// Updates the edge swipe precedence between Chromium native swipe and the
// default WebView swipe for the currently active web state.
- (void)updateEdgeSwipePrecedenceForActiveWebState;

// If an animation for navigating back is necessary, animates, navigate
// back and return YES. Otherwise, do nothing and return NO.
- (BOOL)navigateBackWithSideSwipeAnimationIfNeeded;

// Prepares the view for a slide-in overlay navigation transition in the
// specified direction.
//
// This method sets up for an overlay navigation transition where the entire
// screen is initially positioned offscreen. A snapshot of the screen is passed
// as an argument and used to replace the current fullscreen view, creating a
// seamless slide-in effect when `slideToCenterAnimated` is called.
//
// Important: After calling this method, you must call `slideToCenterAnimated`
// to restore the fullscreen view to its original position and complete the
// transition.
- (void)prepareForSlideInDirection:(UISwipeGestureRecognizerDirection)direction
                     snapshotImage:(UIImage*)snapshotImage;

// Restores the fullscreen view to its original position with an animation.
//
// This method animates the fullscreen view back to its original onscreen
// position after it has been moved offscreen using
// `prepareForSlideInDirection:`.
- (void)slideToCenterAnimated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_SIDE_SWIPE_COMMANDS_H_
