// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_web_view.h"

#import "ios/web/common/crw_input_view_provider.h"

@implementation CRWWebView

#pragma mark - UIResponder

- (UIView*)inputView {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputView)]) {
    UIView* view = [responderInputView inputView];
    if (view) {
      return view;
    }
  }
  return [super inputView];
}

- (UIInputViewController*)inputViewController {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputViewController)]) {
    UIInputViewController* controller =
        [responderInputView inputViewController];
    if (controller) {
      return controller;
    }
  }
  return [super inputViewController];
}

- (UIView*)inputAccessoryView {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputAccessoryView)]) {
    UIView* view = [responderInputView inputAccessoryView];
    if (view) {
      return view;
    }
  }
  return [super inputAccessoryView];
}

- (UIInputViewController*)inputAccessoryViewController {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView
          respondsToSelector:@selector(inputAccessoryViewController)]) {
    UIInputViewController* controller =
        [responderInputView inputAccessoryViewController];
    if (controller) {
      return controller;
    }
  }
  return [super inputAccessoryViewController];
}

@end
