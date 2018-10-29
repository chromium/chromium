// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"

#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
CGFloat const kInputAccessoryHeight = 44.0f;
}  // namespace autofill

@interface FormInputAccessoryViewController ()

// Grey view used as the background of the keyboard to fix
// http://crbug.com/847523
@property(nonatomic, strong) UIView* grayBackgroundView;

// The keyboard replacement view, if any.
@property(nonatomic, weak) UIView* keyboardReplacementView;

// The custom view that should be shown in the input accessory view.
@property(nonatomic, strong) FormInputAccessoryView* customAccessoryView;

// If this view controller is paused it shouldn't add its views to the keyboard.
@property(nonatomic, getter=isPaused) BOOL paused;

// Called when the keyboard will or did change frame.
- (void)keyboardWillOrDidChangeFrame:(NSNotification*)notification;

@end

@implementation FormInputAccessoryViewController {
  // Last registered keyboard rectangle.
  CGRect _keyboardFrame;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;
}

#pragma mark - Life Cycle

- (instancetype)init {
  self = [super init];
  if (self) {
    _suggestionsHaveBeenShown = NO;

    if (IsIPadIdiom()) {
      _grayBackgroundView = [[UIView alloc] init];
      _grayBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
      // This color was obtained by try and error.
      _grayBackgroundView.backgroundColor =
          [[UIColor alloc] initWithRed:206 / 255.f
                                 green:212 / 255.f
                                  blue:217 / 255.f
                                 alpha:1];

      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(keyboardWillOrDidChangeFrame:)
                 name:UIKeyboardWillChangeFrameNotification
               object:nil];
    }
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillOrDidChangeFrame:)
               name:UIKeyboardDidChangeFrameNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
  }
  return self;
}

#pragma mark - Public

- (void)presentView:(UIView*)view {
  if (self.paused) {
    return;
  }
  DCHECK(view);
  DCHECK(!view.superview);
  UIView* keyboardView = [self getKeyboardView];
  view.accessibilityViewIsModal = YES;
  [keyboardView.superview addSubview:view];
  UIView* constrainingView =
      [self recursiveGetKeyboardConstraintView:keyboardView];
  DCHECK(constrainingView);
  view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(view, constrainingView);
  self.keyboardReplacementView = view;
}

#pragma mark - FormInputAccessoryConsumer

- (void)showCustomInputAccessoryView:(UIView*)view
                  navigationDelegate:
                      (id<FormInputAccessoryViewDelegate>)navigationDelegate {
  DCHECK(view);
  if (IsIPadIdiom()) {
    // On iPad, there's no inputAccessoryView available, so we attach the custom
    // view directly to the keyboard view instead.
    [self.customAccessoryView removeFromSuperview];
    [self.grayBackgroundView removeFromSuperview];

    // If the keyboard isn't visible don't show the custom view.
    if (CGRectIntersection([UIScreen mainScreen].bounds, _keyboardFrame)
                .size.height == 0 ||
        CGRectEqualToRect(_keyboardFrame, CGRectZero)) {
      self.customAccessoryView = nil;
      return;
    }

    if (!autofill::features::IsPasswordManualFallbackEnabled()) {
      // If this is a form suggestion view and no suggestions have been
      // triggered yet, don't show the custom view.
      FormSuggestionView* formSuggestionView =
          base::mac::ObjCCast<FormSuggestionView>(view);
      if (formSuggestionView) {
        int numSuggestions = [[formSuggestionView suggestions] count];
        if (!_suggestionsHaveBeenShown && numSuggestions == 0) {
          self.customAccessoryView = nil;
          return;
        }
      }
      _suggestionsHaveBeenShown = YES;
    }
    self.customAccessoryView = [[FormInputAccessoryView alloc] init];
    [self.customAccessoryView setUpWithCustomView:view];
    [self addCustomAccessoryViewIfNeeded];
  } else {
    // On iPhone, the custom view replaces the default UI of the
    // inputAccessoryView.
    [self restoreOriginalInputAccessoryView];
    self.customAccessoryView = [[FormInputAccessoryView alloc] init];
    [self.customAccessoryView setUpWithNavigationDelegate:navigationDelegate
                                               customView:view];
    [self addCustomAccessoryViewIfNeeded];
  }
}

- (void)restoreOriginalKeyboardView {
  [self restoreOriginalInputAccessoryView];
  [self.keyboardReplacementView removeFromSuperview];
  self.keyboardReplacementView = nil;
  self.paused = NO;
}

- (void)pauseCustomKeyboardView {
  [self removeCustomInputAccessoryView];
  [self.keyboardReplacementView removeFromSuperview];
  self.paused = YES;
}

- (void)continueCustomKeyboardView {
  self.paused = NO;
}

- (void)removeAnimationsOnKeyboardView {
  // Work Around. On focus event, keyboardReplacementView is animated but the
  // keyboard isn't. Cancel the animation to match the keyboard behavior
  if (self.keyboardReplacementView.superview) {
    [self.keyboardReplacementView.layer removeAllAnimations];
  }
}

#pragma mark - Private

// Removes the custom views related to the input accessory view.
- (void)removeCustomInputAccessoryView {
  [self.customAccessoryView removeFromSuperview];
  [self.grayBackgroundView removeFromSuperview];
}

// Removes the custom input accessory views and clears the references.
- (void)restoreOriginalInputAccessoryView {
  [self removeCustomInputAccessoryView];
  self.customAccessoryView = nil;
}

// This searches in a keyboard view hierarchy for the best candidate to
// constrain a view to the keyboard.
- (UIView*)recursiveGetKeyboardConstraintView:(UIView*)view {
  for (UIView* subview in view.subviews) {
    // TODO(crbug.com/845472): verify this on iOS 10-12 and all devices.
    // Currently only tested on X-iOS12, 6+-iOS11 and 7+-iOS10. iPhoneX, iOS 11
    // and 12 uses "Dock" and iOS 10 uses "Backdrop". iPhone6+, iOS 11 uses
    // "Dock".
    if ([NSStringFromClass([subview class]) containsString:@"Dock"] ||
        [NSStringFromClass([subview class]) containsString:@"Backdrop"]) {
      return subview;
    }
    UIView* found = [self recursiveGetKeyboardConstraintView:subview];
    if (found) {
      return found;
    }
  }
  return nil;
}

- (UIView*)getKeyboardView {
  NSArray* windows = [UIApplication sharedApplication].windows;
  if (windows.count < 2)
    return nil;

  UIWindow* window;
  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    // TODO(crbug.com/845472): verify this works on iPad with split view before
    // making this the default.
    window = windows.lastObject;
  } else {
    window = windows[1];
  }

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

- (void)keyboardWillShow:(NSNotification*)notification {
  // [iPhone, iOS 10] If the customAccessoryView has not been added to the
  // hierarchy, do it here. This can happen is the view is provided before the
  // keyboard view is created by the system, i.e. the first time the keyboard
  // will appear.
  if (!IsIPadIdiom()) {
    [self addCustomKeyboardViewIfNeeded];
  }
}

- (void)keyboardWillOrDidChangeFrame:(NSNotification*)notification {
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  UIView* keyboardView = [self getKeyboardView];
  CGRect windowRect = keyboardView.window.bounds;
  // On iPad when the keyboard is undocked, on iOS 10 and 11,
  // `UIKeyboard*HideNotification` or `UIKeyboard*ShowNotification` are not
  // being sent. So the check is done here.
  if (CGRectContainsRect(windowRect, keyboardFrame)) {
    _keyboardFrame = keyboardFrame;
  } else {
    _keyboardFrame = CGRectZero;
  }
  // On ipad we hide the views so they don't stick around at the bottom. Only
  // needed on iPad because we add the view directly to the keyboard view.
  if (IsIPadIdiom() && self.customAccessoryView) {
    if (@available(iOS 11, *)) {
    } else {
      // [iPad iOS 10] There is a bug when constraining something to the
      // keyboard view. So this updates the frame instead.
      CGFloat height = autofill::kInputAccessoryHeight;
      self.customAccessoryView.frame =
          CGRectMake(keyboardView.frame.origin.x, -height,
                     keyboardView.frame.size.width, height);
    }
    if (CGRectEqualToRect(_keyboardFrame, CGRectZero)) {
      self.customAccessoryView.hidden = true;
      self.grayBackgroundView.hidden = true;
    } else {
      self.customAccessoryView.hidden = false;
      self.grayBackgroundView.hidden = false;
    }
  }
}

- (void)addCustomKeyboardViewIfNeeded {
  if (self.isPaused) {
    return;
  }
  if (self.keyboardReplacementView && !self.keyboardReplacementView.superview) {
    [self presentView:self.keyboardReplacementView];
  }
}

// Adds the customAccessoryView and the backgroundView (on iPads), if those are
// not already in the hierarchy.
- (void)addCustomAccessoryViewIfNeeded {
  if (self.isPaused) {
    return;
  }
  if (self.customAccessoryView && !self.customAccessoryView.superview) {
    if (IsIPadIdiom()) {
      UIView* keyboardView = [self getKeyboardView];
      // [iPad iOS 10] There is a bug when constraining something to the
      // keyboard view. So this sets the frame instead.
      if (@available(iOS 11, *)) {
        self.customAccessoryView.translatesAutoresizingMaskIntoConstraints = NO;
        [keyboardView addSubview:self.customAccessoryView];
        [NSLayoutConstraint activateConstraints:@[
          [self.customAccessoryView.leadingAnchor
              constraintEqualToAnchor:keyboardView.leadingAnchor],
          [self.customAccessoryView.trailingAnchor
              constraintEqualToAnchor:keyboardView.trailingAnchor],
          [self.customAccessoryView.bottomAnchor
              constraintEqualToAnchor:keyboardView.topAnchor],
          [self.customAccessoryView.heightAnchor
              constraintEqualToConstant:autofill::kInputAccessoryHeight]
        ]];
      } else {
        CGFloat height = autofill::kInputAccessoryHeight;
        self.customAccessoryView.frame =
            CGRectMake(keyboardView.frame.origin.x, -height,
                       keyboardView.frame.size.width, height);
        [keyboardView addSubview:self.customAccessoryView];
      }
      if (!self.grayBackgroundView.superview) {
        [keyboardView addSubview:self.grayBackgroundView];
        [keyboardView sendSubviewToBack:self.grayBackgroundView];
        AddSameConstraints(self.grayBackgroundView, keyboardView);
      }
    } else {
      UIResponder* firstResponder = GetFirstResponder();
      UIView* inputAccessoryView = firstResponder.inputAccessoryView;
      if (inputAccessoryView) {
        [inputAccessoryView addSubview:self.customAccessoryView];
        AddSameConstraints(self.customAccessoryView, inputAccessoryView);
      }
    }
  }
}

@end
