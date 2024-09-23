// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/extension_open_url.h"

bool ExtensionOpenURL(NSURL* url,
                      UIResponder* responder,
                      BlockWithBoolean pre_open_block) {
  while ((responder = responder.nextResponder)) {
    SEL open_url_selector = @selector(openURL:options:completionHandler:);
    if ([responder respondsToSelector:open_url_selector]) {
      if (pre_open_block) {
        pre_open_block(YES);
      }
      NSMethodSignature* method_signature =
          [responder methodSignatureForSelector:open_url_selector];
      NSInvocation* open_invocation =
          [NSInvocation invocationWithMethodSignature:method_signature];
      open_invocation.target = responder;
      open_invocation.selector = open_url_selector;
      [open_invocation setArgument:&url atIndex:2];
      [open_invocation retainArguments];
      [open_invocation invoke];
      return YES;
    }
  }
  if (pre_open_block) {
    pre_open_block(NO);
  }
  return NO;
}
