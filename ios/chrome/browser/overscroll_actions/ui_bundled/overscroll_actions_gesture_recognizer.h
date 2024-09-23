// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_GESTURE_RECOGNIZER_H_
#define IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_GESTURE_RECOGNIZER_H_

#import <UIKit/UIPanGestureRecognizer.h>

// Subclass of UIPanGestureRecognizer that works around a bug where the targets'
// action is not called when the gesture ends while "Speak selection" is
// enabled (crbug.com/699655).
// This subclass works around the bug by calling the action of the target passed
// in the constructor when `reset` is called.
@interface OverscrollActionsGestureRecognizer : UIPanGestureRecognizer
@end

#endif  // IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_UI_BUNDLED_OVERSCROLL_ACTIONS_GESTURE_RECOGNIZER_H_
