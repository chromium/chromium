// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_web_view.h"

#import "ios/web/common/crw_input_view_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWebView

#pragma mark - UIResponder

- (UIView*)inputView {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputView)]) {
    return [responderInputView inputView];
  }
  return [super inputView];
}

- (UIInputViewController*)inputViewController {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputViewController)]) {
    return [responderInputView inputViewController];
  }
  return [super inputViewController];
}

- (UIView*)inputAccessoryView {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputAccessoryView)]) {
    return [responderInputView inputAccessoryView];
  }
  return [super inputAccessoryView];
}

- (UIInputViewController*)inputAccessoryViewController {
  id<CRWResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView
          respondsToSelector:@selector(inputAccessoryViewController)]) {
    return [responderInputView inputAccessoryViewController];
  }
  return [super inputAccessoryViewController];
}

@end
