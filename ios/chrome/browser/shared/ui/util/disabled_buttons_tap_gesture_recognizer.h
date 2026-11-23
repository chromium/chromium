// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_DISABLED_BUTTONS_TAP_GESTURE_RECOGNIZER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_DISABLED_BUTTONS_TAP_GESTURE_RECOGNIZER_H_

#import <UIKit/UIKit.h>

// A tap gesture recognizer intended to be attached to an ancestor container
// (such as a `UIStackView`) that fires only when a tap lands inside one of the
// tracked `buttons` while that button is disabled and visible.
//
// Caller contract:
// - Does not cancel touches in the attached view (`cancelsTouchesInView` is
//   `NO`).
// - Acts as its own `delegate`; callers must not reassign `delegate`.
@interface DisabledButtonsTapGestureRecognizer : UITapGestureRecognizer

// Initializes the recognizer with `target`, `action`, and the `buttons` to
// monitor when disabled (held weakly).
- (instancetype)initWithTarget:(id)target
                        action:(SEL)action
                       buttons:(NSArray<UIButton*>*)buttons
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTarget:(id)target action:(SEL)action NS_UNAVAILABLE;
- (void)setDelegate:(id<UIGestureRecognizerDelegate>)delegate NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_DISABLED_BUTTONS_TAP_GESTURE_RECOGNIZER_H_
