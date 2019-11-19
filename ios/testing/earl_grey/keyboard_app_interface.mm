// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/keyboard_app_interface.h"

#import <UIKit/UIKit.h>
#include <atomic>

#import "base/test/ios/wait_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// EarlGrey fails to detect undocked keyboards on screen, so this help check
// for them.
static std::atomic_bool gCHRIsKeyboardShown(false);

// Returns the dismiss key if present in the passed keyboard layout. Returns nil
// if not found.
UIAccessibilityElement* KeyboardDismissKeyInLayout() {
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  UIAccessibilityElement* key = nil;
  if ([layout accessibilityElementCount] != NSNotFound) {
    for (NSInteger i = [layout accessibilityElementCount]; i >= 0; --i) {
      id element = [layout accessibilityElementAtIndex:i];
      if ([[[element key] valueForKey:@"name"] isEqual:@"Dismiss-Key"]) {
        key = element;
        break;
      }
    }
  }
  return key;
}

// Returns YES if the keyboard is docked at the bottom. NO otherwise.
BOOL IsKeyboardDockedForLayout() {
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  CGRect windowBounds = layout.window.bounds;
  UIView* viewToCompare = layout;
  while (viewToCompare &&
         viewToCompare.bounds.size.height < windowBounds.size.height) {
    CGRect keyboardFrameInWindow =
        [viewToCompare.window convertRect:viewToCompare.bounds
                                 fromView:viewToCompare];

    CGFloat maxY = CGRectGetMaxY(keyboardFrameInWindow);
    if ([@(maxY) isEqualToNumber:@(windowBounds.size.height)]) {
      return YES;
    }
    viewToCompare = viewToCompare.superview;
  }
  return NO;
}

}  // namespace

@implementation KeyboardAppInterface

+ (void)load {
  @autoreleasepool {
    // EarlGrey fails to detect undocked keyboards on screen, so this help check
    // for them.
    auto block = ^(NSNotification* note) {
      CGRect keyboardFrame =
          [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
      UIWindow* window = [UIApplication sharedApplication].keyWindow;
      keyboardFrame = [window convertRect:keyboardFrame fromWindow:nil];
      CGRect windowFrame = window.frame;
      CGRect frameIntersection = CGRectIntersection(windowFrame, keyboardFrame);
      gCHRIsKeyboardShown =
          frameIntersection.size.width > 1 && frameIntersection.size.height > 1;
    };

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidChangeFrameNotification
                    object:nil
                     queue:nil
                usingBlock:block];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidShowNotification
                    object:nil
                     queue:nil
                usingBlock:block];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidHideNotification
                    object:nil
                     queue:nil
                usingBlock:block];
  }
}

+ (BOOL)isKeyboadDocked {
  return IsKeyboardDockedForLayout();
}

+ (id<GREYMatcher>)keyboardWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  UIAccessibilityElement* key = KeyboardDismissKeyInLayout();
  id<GREYMatcher> parentMatcher =
      grey_descendant(grey_accessibilityLabel(key.accessibilityLabel));
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYAction>)keyboardUndockAction {
  UIAccessibilityElement* key = KeyboardDismissKeyInLayout();
  CGRect keyFrameInScreen = [key accessibilityFrame];
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  CGRect keyFrameInWindow = [UIScreen.mainScreen.coordinateSpace
            convertRect:keyFrameInScreen
      toCoordinateSpace:layout.window.coordinateSpace];
  CGRect windowBounds = layout.window.bounds;

  CGPoint startPoint = CGPointMake(
      (keyFrameInWindow.origin.x + keyFrameInWindow.size.width / 2.0) /
          windowBounds.size.width,
      (keyFrameInWindow.origin.y + keyFrameInWindow.size.height / 2.0) /
          windowBounds.size.height);

  return grey_swipeFastInDirectionWithStartPoint(kGREYDirectionUp, startPoint.x,
                                                 startPoint.y);
}

+ (id<GREYAction>)keyboardDockAction {
  UIAccessibilityElement* key = KeyboardDismissKeyInLayout();
  CGRect keyFrameInScreen = [key accessibilityFrame];
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  CGRect keyFrameInWindow = [UIScreen.mainScreen.coordinateSpace
            convertRect:keyFrameInScreen
      toCoordinateSpace:layout.window.coordinateSpace];
  CGRect windowBounds = layout.window.bounds;
  CGPoint startPoint = CGPointMake(
      (keyFrameInWindow.origin.x + keyFrameInWindow.size.width / 2.0) /
          windowBounds.size.width,
      (keyFrameInWindow.origin.y + keyFrameInWindow.size.height / 2.0) /
          windowBounds.size.height);
  return grey_swipeFastInDirectionWithStartPoint(kGREYDirectionDown,
                                                 startPoint.x, startPoint.y);
}

// If the keyboard is not present this will add a text field to the hierarchy,
// make it first responder and return it. If it is already present, this does
// nothing and returns nil.
+ (UITextField*)showKeyboard {
  UITextField* textField = nil;
  if (!gCHRIsKeyboardShown) {
    CGRect rect = CGRectMake(0, 0, 300, 100);
    textField = [[UITextField alloc] initWithFrame:rect];
    textField.backgroundColor = [UIColor blueColor];
    [[[UIApplication sharedApplication] keyWindow] addSubview:textField];
    [textField becomeFirstResponder];
  }

  ConditionBlock conditionBlock = ^bool {
    return gCHRIsKeyboardShown;
  };
  base::test::ios::TimeUntilCondition(
      nil, conditionBlock, false,
      base::TimeDelta::FromSeconds(base::test::ios::kWaitForUIElementTimeout));
  return textField;
}

@end
