// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/keyboard_observer_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface KeyboardObserverHelper ()

// Flag that indicates if the keyboard is on screen.
@property(nonatomic, getter=isKeyboardOnScreen) BOOL keyboardOnScreen;

// Flag that indicates if the next keyboard did hide notification should be
// ignored. This happens when the keyboard is on screen and the device rotates.
// Causing keyboard notifications to be sent, but the keyboard never leaves the
// screen.
@property(nonatomic, getter=shouldIgnoreNextKeyboardDidHide)
    BOOL ignoreNextKeyboardDidHide;

@end

@implementation KeyboardObserverHelper

- (instancetype)init {
  self = [super init];
  if (self) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillHide:)
               name:UIKeyboardWillHideNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardDidHide:)
               name:UIKeyboardDidHideNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(orientationDidChange:)
               name:UIApplicationDidChangeStatusBarOrientationNotification
             object:nil];
  }
  return self;
}

- (void)keyboardWillShow:(NSNotification*)notification {
  self.keyboardOnScreen = YES;
}

- (void)keyboardWillHide:(NSNotification*)notification {
  self.keyboardOnScreen = NO;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (self.keyboardOnScreen) {
      [self.delegate keyboardDidStayOnScreen];
    }
  });
}

- (void)keyboardDidHide:(NSNotification*)notification {
  // If UIKeyboardDidHideNotification was sent because of a orientation
  // change, reset the flag and ignore.
  if (self.shouldIgnoreNextKeyboardDidHide) {
    self.ignoreNextKeyboardDidHide = NO;
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!self.keyboardOnScreen) {
      [self.delegate keyboardDidHide];
    }
  });
}

- (void)orientationDidChange:(NSNotification*)notification {
  // If the keyboard is on screen, set the flag to ignore next keyboard did
  // hide.
  if (self.keyboardOnScreen) {
    self.ignoreNextKeyboardDidHide = YES;
  }
}

@end
