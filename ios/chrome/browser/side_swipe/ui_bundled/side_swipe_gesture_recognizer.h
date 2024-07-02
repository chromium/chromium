// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_GESTURE_RECOGNIZER_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_GESTURE_RECOGNIZER_H_

#import <UIKit/UIGestureRecognizerSubclass.h>
#import <UIKit/UIKit.h>

// This gesture recognizer is used because the default swipe gesture recognizer
// is too restrictive.  The default swipe requires a particular velocity and
// distance that Chrome does not require.
// Another benefit of using a custom gesture recognizer is it can fail faster,
// so recognizers that depend on this recognizer can begin faster.
@interface SideSwipeGestureRecognizer : UIPanGestureRecognizer

// The distance from the edge of the screen in which a side swipe can start.
@property(nonatomic, assign) CGFloat swipeEdge;
// The distance from the edge of the screen from which a gesture starts.
@property(readonly, nonatomic) CGFloat swipeOffset;
// The distance between touches for a swipe to begin.
@property(readwrite, nonatomic) CGFloat swipeThreshold;
// Starting point of swipe.
@property(readonly, nonatomic) CGPoint startPoint;
@property(nonatomic, assign) UISwipeGestureRecognizerDirection direction;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_GESTURE_RECOGNIZER_H_
