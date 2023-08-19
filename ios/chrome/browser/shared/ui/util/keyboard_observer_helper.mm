// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

@interface KeyboardObserverHelper ()

// Flag that indicates if the keyboard is on screen.
@property(nonatomic, readwrite, getter=isKeyboardVisible) BOOL keyboardVisible;

@end

@implementation KeyboardObserverHelper

#pragma mark - Public class methods

+ (instancetype)sharedKeyboardObserver {
  static KeyboardObserverHelper* sharedInstance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[KeyboardObserverHelper alloc] init];
  });
  return sharedInstance;
}

+ (CGFloat)keyboardHeightInWindow:(UIWindow*)window {
  if (!window) {
    return 0;
  }
  UIView* keyboardView = [KeyboardObserverHelper keyboardViewInWindow:window];
  CGRect keyboardFrame = keyboardView.frame;
  BOOL keyboardCoversFullWidth =
      CGRectGetWidth(keyboardFrame) >= CGRectGetWidth(window.bounds);
  BOOL isDocked = CGRectGetMaxY(keyboardFrame) >= CGRectGetMaxY(window.bounds);
  if (!keyboardCoversFullWidth || !isDocked) {
    return 0;
  }
  CGRect intersection = CGRectIntersection(keyboardFrame, window.bounds);
  return CGRectGetHeight(intersection);
}

#pragma mark - Private instance methods

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
    _keyboardVisible = NO;
  }
  return self;
}

#pragma mark - Private class methods

// Returns the keyboard view in `window`.
// keyboard view coordinates {x: keyboard x in window coordinate,
//                            y: keyboard y in window coordinate}
// When the keyboard is docked and software (the keyboard spans across the full
// width of the screen):
// - Fixed window (full screen, split screen):
//     keyboard size is {width: screen width,
//                       height: keyboard height}
// - Floating window (slide over, stage manager):
//     keyboard size is {width: window width,
//                       height: keyboard and window intersection height}
// When the keyboard is hardware:
// - Fixed window (full screen, split screen):
//     keyboard view size is {width: screen width,
//                            height: accessory height}
// - Slide over:
//     keyboard view size is {width: window width,
//                           height: accessory and window intersection height}
// - Stage manager:
//     keyboard view size is {width: window width, height: 0}
+ (UIView*)keyboardViewInWindow:(UIWindow*)window {
  if (!window || !window.windowScene || !window.windowScene.windows.count) {
    return nil;
  }

  // Iterate windows in reverse order from frontmost to back.
  for (UIWindow* w in window.windowScene.windows.reverseObjectEnumerator) {
    for (UIView* subview in w.subviews) {
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
  }

  return nil;
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  self.keyboardVisible = YES;
}

- (void)keyboardWillHide:(NSNotification*)notification {
  self.keyboardVisible = NO;
}

@end
