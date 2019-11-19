// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_KEYBOARD_APP_INTERFACE_H_
#define IOS_TESTING_EARL_GREY_KEYBOARD_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@protocol GREYAction;
@protocol GREYMatcher;

// KeyboardAppInterface contains helpers for interacting with the keyboard.
// These are compiled into the app binary and can be called from either app or
// test code.
@interface KeyboardAppInterface : NSObject

// Return a boolean indicating if the keyboard is docked.
+ (BOOL)isKeyboadDocked;

// Matcher for the Keyboard Window.
+ (id<GREYMatcher>)keyboardWindowMatcher;

// Swipe action to undock the keyboard.
+ (id<GREYAction>)keyboardUndockAction;

// Swipe action to dock the keyboard.
+ (id<GREYAction>)keyboardDockAction;

// If the keyboard is not present this will add a text field to the hierarchy,
// make it first responder and return it. If it is already present, this does
// nothing and returns nil.
+ (UITextField*)showKeyboard;

@end

#endif  // IOS_TESTING_EARL_GREY_KEYBOARD_APP_INTERFACE_H_
