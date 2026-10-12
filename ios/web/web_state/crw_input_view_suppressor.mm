// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_input_view_suppressor.h"

namespace {

// Returns the first responder in `view` or its subview hierarchy, or `nil` if
// no view in the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder]) {
    return view;
  }

  for (UIView* subview in [view subviews]) {
    UIView* first_responder = GetFirstResponderSubview(subview);
    if (first_responder) {
      return first_responder;
    }
  }

  return nil;
}

}  // namespace

@implementation CRWInputViewSuppressor {
  // Owning view.
  __weak UIView* _view;
  // Zero-sized view that replaces the system software keyboard.
  UIView* _emptyInputView;
}

#pragma mark - Public

- (instancetype)initWithView:(UIView*)view {
  if ((self = [super init])) {
    _view = view;
    _emptyInputView = [[UIView alloc] initWithFrame:CGRectZero];
  }
  return self;
}

- (void)reloadInputViews {
  // Call `reloadInputViews` on the active first responder in the hierarchy,
  // as UIKit ignores it on non-first-responders.
  [GetFirstResponderSubview(_view) reloadInputViews];
}

- (void)disconnect {
  [self reloadInputViews];
  _view = nil;
}

#pragma mark - CRWResponderInputView

- (UIView*)inputView {
  // Return a zero-sized `UIView` to hide the software keyboard.
  return _emptyInputView;
}

- (UIInputViewController*)inputViewController {
  return nil;
}

- (UIView*)inputAccessoryView {
  // Returning `nil` hides the form accessory toolbar when a web input element
  // is focused.
  return nil;
}

- (UIInputViewController*)inputAccessoryViewController {
  return nil;
}

@end
