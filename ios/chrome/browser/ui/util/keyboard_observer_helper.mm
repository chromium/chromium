// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface KeyboardObserverHelper ()

// Flag that indicates if the keyboard is on screen.
@property(nonatomic, readwrite, getter=isKeyboardVisible) BOOL keyboardVisible;

// Current keyboard state.
@property(nonatomic, readwrite, getter=getKeyboardState)
    KeyboardState keyboardState;

// The last known keyboard view. If this changes, it probably means that the
// application lost focus in multiwindow mode.
@property(nonatomic, weak) UIView* keyboardView;

// Mutable array storing weak pointers to consumers.
@property(nonatomic, strong) NSPointerArray* consumers;

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

#pragma mark - Public instance methods

- (void)addConsumer:(id<KeyboardObserverHelperConsumer>)consumer {
  [self.consumers addPointer:(__bridge void*)consumer];
  [self.consumers compact];
}

- (CGFloat)visibleKeyboardHeight {
  if (self.keyboardState.isVisible && !self.keyboardState.isHardware &&
      !self.keyboardState.isUndocked) {
    // Software keyboard is visible and covers the full width of the screen
    // (docked). Returns the keyboard + accessory height.
    return CGRectGetHeight(self.keyboardView.frame);
  } else if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE &&
             self.keyboardState.isVisible && !self.keyboardState.isUndocked) {
    // Keyboard is visible but hardware, only the accessory covers the full
    // width of the display, the keyboard is hidden below the display. Returns
    // the accessory's height.
    return CurrentScreenHeight() - self.keyboardView.frame.origin.y;
  } else {
    return 0;
  }
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
    _consumers =
        [[NSPointerArray alloc] initWithOptions:NSPointerFunctionsWeakMemory];
  }
  return self;
}

#pragma mark - Private class methods

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

+ (UIScreen*)keyboardScreen {
  UIView* keyboardView = [self keyboardView];
  return keyboardView.window.screen;
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  self.keyboardVisible = YES;
}

- (void)keyboardWillHide:(NSNotification*)notification {
  self.keyboardVisible = NO;
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
    // Notify on the next cycle.
    dispatch_async(dispatch_get_main_queue(), ^{
      for (id<KeyboardObserverHelperConsumer> consumer in self.consumers) {
        [consumer keyboardWillChangeToState:self.keyboardState];
      }
    });
  }
}

@end
