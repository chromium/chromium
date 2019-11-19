// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

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
  NSUInteger expectedMinWindows = IsIPadIdiom() ? 2 : 3;
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

+ (id<EdgeLayoutGuideProvider>)keyboardLayoutGuide {
  // [iOS 13] Sometimes there is an input assistant provided by the system.
  // I.e. autocorrection or passwords. A composed guide is needed because
  // there isn't a view "hugging" the accessory and the keyboard. This used to
  // be merged with the keyboard < iOS 13.
  //
  // [iOS 13][iOS 12][iPhone][iPhoneX]: "Backdrop" works even when a picker is
  // present.
  for (NSString* name in @[ @"InputAssistant", @"Backdrop" ]) {
    id<EdgeLayoutGuideProvider> layout =
        [self keyboardLayoutGuideInHostView:self.keyboardView withName:name];
    if (layout) {
      return layout;
    }
  }
  return nil;
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillDidChangeFrame:(NSNotification*)notification {
  [self updateKeyboardState];
}

- (void)keyboardWillShow:(NSNotification*)notification {
  self.keyboardOnScreen = YES;
}

- (void)keyboardWillHide:(NSNotification*)notification {
  self.keyboardOnScreen = NO;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (self.keyboardOnScreen) {
      [self.consumer keyboardDidStayOnScreen];
    }
  });
}

#pragma mark - Private

// Returns a layout guide defined by the top edge of |view| and its window.
+ (id<EdgeLayoutGuideProvider>)topEdgeLayoutGuideForView:(UIView*)view {
  ComposedEdgeLayoutGuide* layoutGuide = [[ComposedEdgeLayoutGuide alloc] init];
  layoutGuide.baseLayoutGuide = view.window;
  layoutGuide.topAnchorProvider = view;
  return layoutGuide;
}

// This searches in the passed view hierarchy for the best Layout Guide for the
// keyboard. Unexpected behaviour on iPad.
+ (id<EdgeLayoutGuideProvider>)keyboardLayoutGuideInHostView:(UIView*)hostView
                                                    withName:(NSString*)name {
  DCHECK(!IsIPadIdiom());

  for (UIView* subview in hostView.subviews) {
    if ([NSStringFromClass([subview class]) containsString:name]) {
      return [self topEdgeLayoutGuideForView:subview];
    }
    // Continue searching recursively.
    id<EdgeLayoutGuideProvider> found =
        [self keyboardLayoutGuideInHostView:subview withName:name];
    if (found) {
      return found;
    }
  }

  return nil;
}

#pragma mark Keyboard State Detection

// Update keyboard state by looking at keyboard frame and the existence of some
// classes to detect split view or pickers.
- (void)updateKeyboardState {
  UIView* keyboardView = KeyboardObserverHelper.keyboardView;

  CGFloat windowHeight = [UIScreen mainScreen].bounds.size.height;
  CGRect keyboardFrame = keyboardView.frame;
  BOOL isVisible = CGRectGetMinY(keyboardFrame) < windowHeight;
  BOOL isUndocked = CGRectGetMaxY(keyboardFrame) < windowHeight;
  BOOL isHardware = isVisible && CGRectGetMaxY(keyboardFrame) > windowHeight;
  BOOL isSplit = [self viewIsSplit:keyboardView];
  BOOL isPicker = [self containsPickerView:keyboardView];

  // Only notify if a change is detected.
  if (isVisible != self.keyboardState.isVisible ||
      isUndocked != self.keyboardState.isUndocked ||
      isSplit != self.keyboardState.isSplit ||
      isHardware != self.keyboardState.isHardware ||
      isPicker != self.keyboardState.isPicker) {
    self.keyboardState = {isVisible, isUndocked, isSplit, isHardware, isPicker};
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.consumer keyboardWillChangeToState:self.keyboardState];
    });
  }
}

// Checks for a picker UIView* under the given |view|.
- (BOOL)containsPickerView:(UIView*)view {
  for (UIView* subview in view.subviews) {
    if ([NSStringFromClass([subview class]) rangeOfString:@"Picker"].location !=
        NSNotFound) {
      return YES;
    }
  }
  return NO;
}

// Checks for the presence of split image views under the given |view|.
- (BOOL)viewIsSplit:(UIView*)view {
  // Don't waste time going through the accessory views.
  if ([NSStringFromClass([view class]) rangeOfString:@"FormInputAccessoryView"]
          .location != NSNotFound) {
    return NO;
  }

  for (UIView* subview in view.subviews) {
    if ([NSStringFromClass([subview class]) rangeOfString:@"SplitImage"]
            .location != NSNotFound) {
      return subview.subviews.count > 1;
    }
    if ([self viewIsSplit:subview])
      return YES;
  }

  return NO;
}

@end
