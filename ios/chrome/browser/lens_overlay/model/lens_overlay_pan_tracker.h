// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PAN_TRACKER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PAN_TRACKER_H_

#import <UIKit/UIKit.h>

// Utility class that tracks whether the user pans a given UIView.
//
// Does not strongly retain the view.
@interface LensOverlayPanTracker : NSObject <UIGestureRecognizerDelegate>

// Whether the user pans the view.
@property(nonatomic, assign, readonly) BOOL isPanning;

// Creates a new instance of the pan tracker for a given UIView.
- (instancetype)initWithView:(UIView*)view;

// Starts tracking pan gestures on the given view.
- (void)startTracking;

// Stops tracking pan gestures on the given view.
- (void)stopTracking;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PAN_TRACKER_H_
