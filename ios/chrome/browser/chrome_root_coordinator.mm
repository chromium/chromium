// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_root_coordinator.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeRootCoordinator
@synthesize window = _window;

- (instancetype)initWithWindow:(UIWindow*)window {
  if ((self = [super initWithBaseViewController:nil browser:nullptr])) {
    _window = window;
  }
  return self;
}

@end
