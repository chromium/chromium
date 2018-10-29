// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_KEYBOARD_OBSERVER_HELPER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_KEYBOARD_OBSERVER_HELPER_H_

#import <UIKit/UIKit.h>

// Delegate informed about the visible/hidden state of the keyboard.
@protocol KeyboardObserverHelperDelegate<NSObject>

// Indicates that |UIKeyboardWillHideNotification| was posted but the keyboard
// was not hidden. For example, this can happen when jumping between fields.
- (void)keyboardDidStayOnScreen;

// Indicates that |UIKeyboardWillHideNotification| was posted and the keyboard
// was actually dismissed.
- (void)keyboardDidHide;

@end

// Helper to observe the keyboard and report updates.
@interface KeyboardObserverHelper : NSObject

// The delegate to inform of the keyboard state changes.
@property(nonatomic, weak) id<KeyboardObserverHelperDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_KEYBOARD_OBSERVER_HELPER_H_
