// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"

@implementation ScrollDelegateProxy

- (instancetype)initWithInterceptingTarget:
                    (NSObject<UIScrollViewDelegate>*)interceptingTarget
                            originalTarget:(NSObject<UIScrollViewDelegate>*)
                                               originalTarget {
  _interceptingTarget = interceptingTarget;
  _originalTarget = originalTarget;
  return self;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  if ([_interceptingTarget respondsToSelector:aSelector]) {
    return YES;
  }
  if ([_originalTarget respondsToSelector:aSelector]) {
    return YES;
  }
  return NO;
}

- (NSMethodSignature*)methodSignatureForSelector:(SEL)aSelector {
  NSMethodSignature* signature = nil;
  if ([_interceptingTarget respondsToSelector:aSelector]) {
    signature = [_interceptingTarget methodSignatureForSelector:aSelector];
  }
  if (!signature && [_originalTarget respondsToSelector:aSelector]) {
    signature = [_originalTarget methodSignatureForSelector:aSelector];
  }
  return signature;
}

- (void)forwardInvocation:(NSInvocation*)anInvocation {
  SEL selector = [anInvocation selector];

  if ([_interceptingTarget respondsToSelector:selector]) {
    [anInvocation invokeWithTarget:_interceptingTarget];
  }
  if ([_originalTarget respondsToSelector:selector]) {
    [anInvocation invokeWithTarget:_originalTarget];
  }
}

@end
