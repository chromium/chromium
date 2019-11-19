// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/test_native_content.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestNativeContent {
  GURL _URL;
  GURL _virtualURL;
  UIView* _view;
}
- (instancetype)initWithURL:(const GURL&)URL
                 virtualURL:(const GURL&)virtualURL {
  self = [super init];
  if (self) {
    _URL = URL;
    _virtualURL = virtualURL;
  }
  return self;
}

- (BOOL)respondsToSelector:(SEL)selector {
  if (selector == @selector(virtualURL)) {
    return _virtualURL.is_valid();
  }
  return [super respondsToSelector:selector];
}

- (NSString*)title {
  return @"Test Title";
}

- (const GURL&)url {
  return _URL;
}

- (const GURL&)virtualURL {
  return _virtualURL;
}

- (UIView*)view {
  return nil;
}

- (BOOL)isViewAlive {
  return YES;
}

- (void)reload {
}
@end
