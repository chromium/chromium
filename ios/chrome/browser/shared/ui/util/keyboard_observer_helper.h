// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_

#import <UIKit/UIKit.h>

// Helper to observe the keyboard and report updates.
@interface KeyboardObserverHelper : NSObject

// Singleton for KeyboardObserverHelper.
+ (instancetype)sharedKeyboardObserver;

- (instancetype)init NS_UNAVAILABLE;

// Flag that indicates if the docked software keyboard is visible. Undocked,
// floating and split keyboard are considered hidden even if they are on the
// screen. Note: The hardware keyboard is considered visible when a text field
// becomes first responder. If the software keyboard is shown then hidden, the
// hardware keyboard is considered hidden.
@property(nonatomic, readonly, getter=isKeyboardVisible) BOOL keyboardVisible;

// Returns keyboard's height if it's docked, otherwise returns 0. Note: This
// includes the keyboard accessory's height, with an exception on iPad with
// stage manager enabled. (cf. keyboardViewInWindow)
+ (CGFloat)keyboardHeightInWindow:(UIWindow*)window;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_H_
