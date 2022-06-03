// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

#include "base/check.h"
#include "base/check_op.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/974226): look into making this a singleton with multiple
// listeners.
@interface KeyboardObserverHelper ()

// Flag that indicates if the keyboard is on screen.
@property(nonatomic, readwrite, getter=isKeyboardOnScreen)
    BOOL keyboardOnScreen;

// Current keyboard state.
@property(nonatomic, readwrite, getter=getKeyboardState)
    KeyboardState keyboardState;

// The last known keyboard view. If this changes, it probably means that the
// application lost focus in multiwindow mode.
@property(nonatomic, weak) UIView* keyboardView;

@end

@implementation KeyboardObserverHelper

#pragma mark - Public

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
           selector:@selector(keyboardWillDidChangeFrame:)
               name:UIKeyboardWillChangeFrameNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillDidChangeFrame:)
               name:UIKeyboardDidChangeFrameNotification
             object:nil];
  }
  return self;
}

+ (UIView*)keyboardView {
  NSArray* windows = [UIApplication sharedApplication].windows;
  NSUInteger expectedMinWindows =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) ? 2 : 3;
  if (windows.count < expectedMinWindows)
    return nil;

  UIWindow* window = windows.lastObject;

  for (UIView* subview in window.subviews) {
    if ([NSStringFromClass([subview class]) rangeOfString:@"PeripheralHost"]
            .location != NSNotFound) {
      return subview;
    }
    if ([NSStringFromClass([subview class]) rangeOfString:@"SetContainer"]
            .location != NSNotFound) {
      for (UIView* subsubview in subview.subviews) {
        if ([NSStringFromClass([subsubview class]) rangeOfString:@"SetHost"]
                .location != NSNotFound) {
          return subsubview;
        }
      }
    }
  }

  return nil;
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  self.keyboardOnScreen = YES;
}

- (void)keyboardWillHide:(NSNotification*)notification {
  self.keyboardOnScreen = NO;
}

- (void)keyboardWillDidChangeFrame:(NSNotification*)notification {
  [self updateKeyboardState];
}

#pragma mark Keyboard State Detection

// Update keyboard state by looking at keyboard frame.
- (void)updateKeyboardState {
  UIView* keyboardView = KeyboardObserverHelper.keyboardView;

  CGFloat windowHeight = [UIScreen mainScreen].bounds.size.height;
  CGRect keyboardFrame = keyboardView.frame;
  BOOL isVisible = CGRectGetMinY(keyboardFrame) < windowHeight;
  BOOL isUndocked = CGRectGetMaxY(keyboardFrame) < windowHeight;
  BOOL isHardware = isVisible && CGRectGetMaxY(keyboardFrame) > windowHeight;

  // Only notify if a change is detected.
  if (isVisible != self.keyboardState.isVisible ||
      isUndocked != self.keyboardState.isUndocked ||
      isHardware != self.keyboardState.isHardware ||
      keyboardView != self.keyboardView) {
    self.keyboardState = {isVisible, isUndocked, isHardware};
    self.keyboardView = keyboardView;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.consumer keyboardWillChangeToState:self.keyboardState];
    });
  }
}

@end
