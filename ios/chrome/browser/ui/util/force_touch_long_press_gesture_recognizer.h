// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_FORCE_TOUCH_LONG_PRESS_GESTURE_RECOGNIZER_H_
#define IOS_CHROME_BROWSER_UI_UTIL_FORCE_TOUCH_LONG_PRESS_GESTURE_RECOGNIZER_H_

#import <UIKit/UIKit.h>

// Gesture recognizer triggering on LongPress or on ForceTouch. Any of the two
// gesture can trigger the recognizer. The properties of the LongPress are not
// taken into account for the ForceTouch.
@interface ForceTouchLongPressGestureRecognizer : UILongPressGestureRecognizer

// The threshold at which the force touch should trigger. Must be between 0 and
// 1, based on the maximum force the touch can receive.
@property(nonatomic, assign) CGFloat forceThreshold;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_FORCE_TOUCH_LONG_PRESS_GESTURE_RECOGNIZER_H_
